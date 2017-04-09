#include <cnet.h>
#include <stdlib.h>
#include <string.h>

/*  This is an implementation of a stop-and-wait data link protocol.
    It is based on Tanenbaum's `protocol 4', 2nd edition, p227
    (or his 3rd edition, p205).
    This protocol employs only data and acknowledgement frames -
    piggybacking and negative acknowledgements are not used.

    It is currently written so that only one node (number 0) will
    generate and transmit messages and the other (number 1) will receive
    them. This restriction seems to best demonstrate the protocol to
    those unfamiliar with it.
    The restriction can easily be removed by "commenting out" the line

	    if(nodeinfo.nodenumber == 0)

    in reboot_node(). Both nodes will then transmit and receive (why?).

    Note that this file only provides a reliable data-link layer for a
    network of 2 nodes.
 */

typedef enum    { DL_DATA, DL_ACK }   FRAMEKIND;

typedef struct {
    char        data[MAX_MESSAGE_SIZE];
} MSG;

typedef struct {
    FRAMEKIND    kind;      	// only ever DL_DATA or DL_ACK
    size_t	 len;       	// the length of the msg field only
    int          checksum;  	// checksum of the whole frame
    int          seq;       	// only ever 0 or 
	CnetAddr srcaddr;
    MSG          msg;
} FRAME;

#define FRAME_HEADER_SIZE  (sizeof(FRAME) - sizeof(MSG))
#define FRAME_SIZE(f)      (FRAME_HEADER_SIZE + f.len)


static  MSG       	*lastmsg;
static  size_t		lastlength		= 0;
static  CnetTimerID	lasttimer		= NULLTIMER;

int NCS = 64;
static  int       	ackexpected[301] = { 0 };
static	int		nextframetosend[301] = { 0 };
static	int		frameexpected[3] = { 0 };

#define CHECK_C(call)   if ((call) != 0) LAB_warn (__FILE__, __func__, __LINE__);
void LAB_warn(const char *filenm, const char *function, int lineno)
{
	char   msg[240];
	sprintf(msg, "Warning: %s (%s usec, '%s:%d'): %s",
		nodeinfo.nodename, CNET_format64(nodeinfo.time_in_usec),
		function, lineno, cnet_errname[cnet_errno]);
	fprintf(stderr, "%s\n", msg);
}

const char* typeNames[] = { "DL_DATA", "DL_ACK"};

static void transmit_frame(MSG *msg, FRAMEKIND kind, size_t length, int seqno,int link)
{
    FRAME       f;
    f.kind      = kind;
    f.seq       = seqno;
    f.checksum  = 0;
    f.len       = length;
	
		switch (kind) {
		case DL_ACK:
			printf("ACK transmitted, seq=%d\n", seqno);
			break;

		case DL_DATA: {
		
			CnetTime	timeout;

			printf(" DATA transmitted, seq=%d\n", seqno);
			memcpy(&f.msg, msg, (int)length);

			timeout = FRAME_SIZE(f)*((CnetTime)8000000 / linkinfo[link].bandwidth) +
				linkinfo[link].propagationdelay;

			lasttimer = CNET_start_timer(EV_TIMER1, 3 * timeout, 0);
			break;
		}
		}

	
   
    length      = FRAME_SIZE(f);
    f.checksum  = CNET_ccitt((unsigned char *)&f, (int)length);
    CHECK(CNET_write_physical_reliable(link, &f, &length));
}

static EVENT_HANDLER(application_ready)
{
    CnetAddr destaddr;

    lastlength  = sizeof(MSG);
    CHECK(CNET_read_application(&destaddr, lastmsg, &lastlength));
    CNET_disable_application(300);

    printf("down from application, seq=%d\n", nextframetosend[nodeinfo.address]);
    transmit_frame(lastmsg, DL_DATA, lastlength, nextframetosend[nodeinfo.address],1);
    nextframetosend[nodeinfo.address] = 1-nextframetosend[nodeinfo.address];
}

static EVENT_HANDLER(physical_ready)
{
    FRAME        f;
    size_t	 len;
    int          link, checksum;
	int outLink = (link == 1) ? nodeinfo.nlinks : 1;
    len         = sizeof(FRAME);

    CHECK(CNET_read_physical(&link, &f, &len));
	

	if (nodeinfo.nodetype != NT_ROUTER)
	{
		checksum = f.checksum;
		f.checksum = 0;
		if (CNET_ccitt((unsigned char *)&f, (int)len) != checksum) {
			printf("\t\t\t\tBAD checksum - frame ignored\n");
			return;           // bad checksum, ignore frame
		}

		switch (f.kind) {
		case DL_ACK:
			if (f.seq == ackexpected[nodeinfo.address] && nodeinfo.address !=300) {
				printf("\t\t\t\tACK received, seq=%d\n", f.seq);
				CNET_stop_timer(lasttimer);
				ackexpected[nodeinfo.address] = 1 - ackexpected[nodeinfo.address];
				CNET_enable_application(300);
			}
			break;

		case DL_DATA:
			printf("\t\t\t\tDATA received, seq=%d, ", f.seq);
			if (f.seq == frameexpected[link] && nodeinfo.address == 300) {
				printf("up to application\n");
				len = f.len;
				CHECK_C(CNET_write_application(&f.msg, &len));
				frameexpected[link] = 1 - frameexpected[link];
			}
			else
				printf("ignored\n");
			transmit_frame(NULL, DL_ACK, 0, f.seq,link);
			break;
		}
	}
	else {
		if (link == 1)
		{
			outLink = 2;
		}
		else if (link == 2)
		{
			outLink = 1;
		}
		printf("\t\t\t\tFORWARDING DATA incominglink =%d type=%s seq=%d,link = %d \n",link, typeNames[f.kind], f.seq,outLink);
		transmit_frame(&f.msg, f.kind, f.len, f.seq, outLink);
	}

}

static EVENT_HANDLER(timeouts)
{
    printf("timeout, seq=%d\n", ackexpected[nodeinfo.address]);
	if (nodeinfo.address != 300 && nodeinfo.nodetype != NT_ROUTER)
	{
		transmit_frame(lastmsg, DL_DATA, lastlength, ackexpected[nodeinfo.address], 1);
	}
		
}

static EVENT_HANDLER(showstate)
{
    printf(
    "\n\tackexpected\t= %d\n\tnextframetosend\t= %d\n\tframeexpected\t= %d\n",
		    ackexpected[nodeinfo.address], nextframetosend[nodeinfo.address], frameexpected[nodeinfo.address]);
}

EVENT_HANDLER(reboot_node)
{
    if(nodeinfo.nodenumber > NCS) {
	fprintf(stderr,"This is not a 10-node network!\n");
	exit(1);
    }

    lastmsg	= calloc(1, sizeof(MSG));

	if (nodeinfo.nodetype == NT_HOST && nodeinfo.address != 300 )
	{
		CHECK(CNET_set_handler(EV_APPLICATIONREADY, application_ready, 0));
	}
  
    CHECK(CNET_set_handler( EV_PHYSICALREADY,    physical_ready, 0));
    CHECK(CNET_set_handler( EV_TIMER1,           timeouts, 0));
    CHECK(CNET_set_handler( EV_DEBUG0,           showstate, 0));

    CHECK(CNET_set_debug_string( EV_DEBUG0, "State"));

	CNET_enable_application(300);
}
