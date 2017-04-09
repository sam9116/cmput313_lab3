#include <cnet.h>
#include <assert.h>
#include <string.h>

// This file is based on "protocol.c"
// ------------------------------

#define CHECK_C(call)   if ((call) != 0) LAB_warn (__FILE__, __func__, __LINE__);

void LAB_warn(const char *filenm, const char *function, int lineno)
{
	char   msg[240];
	sprintf(msg, "Warning: %s (%s usec, '%s:%d'): %s",
		nodeinfo.nodename, CNET_format64(nodeinfo.time_in_usec),
		function, lineno, cnet_errname[cnet_errno]);
	fprintf(stderr, "%s\n", msg);
}



#define MAX_DEGREE 32;

typedef enum { DL_HELLO, DL_HELLO_ACK } FRAMEKIND;

typedef struct {
  char         data[20];
} MSG;

typedef struct {
  FRAMEKIND    kind;
  CnetAddr     srcAddr;
  CnetTime     time_send;
  MSG          msg;
} FRAME;    

// --------------------
static EVENT_HANDLER(application_ready)
{
    CnetAddr	destaddr;
    char	buffer[MAX_MESSAGE_SIZE];
    size_t	length;

    length = sizeof(buffer);
    CNET_read_application(&destaddr, buffer, &length);
    printf("\tI have a message of %4d bytes for address %d\n",
			    length, (int)destaddr);
}
// ------------------------------

static EVENT_HANDLER(button_pressed)
{
    printf("\n Node name       : %s\n",	nodeinfo.nodename);
    printf(" Node number     : %d\n",	nodeinfo.nodenumber);
    printf(" Node address    : %d\n",	nodeinfo.address);
    printf(" Number of links : %d\n\n",	nodeinfo.nlinks);

	int j = nodeinfo.nlinks;
	for (int i = 1; i <= j; i++)
	{
		int   link = i;
		size_t  len;
		FRAME f;

		f.kind = DL_HELLO;
		f.srcAddr = nodeinfo.address;
		f.time_send = nodeinfo.time_in_usec;    // get current time

		len = sizeof(f);
		CHECK_C(CNET_write_physical(link, (char *)&f, &len));
		CNET_start_timer(EV_TIMER1, 1000000, 0);   // send continuous hello
	}
}
// ------------------------------
static void physical_ready(CnetEvent ev, CnetTimerID timer, CnetData data)
{

  int    link;
  size_t len;
  //CnetTime  delta;
  FRAME  f;

  len= sizeof(FRAME);
  CHECK ( CNET_read_physical (&link, (char *) &f, &len) );

  switch (f.kind) {
  case DL_HELLO:
	  //assert ( link == 1 );
      f.kind= DL_HELLO_ACK;
	  f.srcAddr = nodeinfo.address;
	  strcpy(f.msg.data, nodeinfo.nodename);
      len= sizeof(f);
	 
      CHECK_C( CNET_write_physical(link, (char *) &f, &len) );
	  //free(f.msg.data[0]);
      break;

  case DL_HELLO_ACK:

	  printf(" Machine Link:%i \n", link);
	  //printf("machine address: %s \n", nodeinfo.)
	  printf(" Machine Name: %s \n", f.msg.data);
	  printf(" Machine Adress: %i \n\n", f.srcAddr);
	  //printf("debug: delta= %lld \n", delta);
      break;
  } 
}
// ------------------------------

// ------------------------------
EVENT_HANDLER(reboot_node)
{
//  Interested in hearing about:
//    - the Application Layer having messages for delivery
//    - timer 1
//

    CNET_set_handler(EV_APPLICATIONREADY, application_ready, 0);

    CNET_set_handler(EV_DEBUG0, button_pressed, 0);
    CNET_set_debug_string(EV_DEBUG0, "Node Info");

    CNET_set_handler(EV_PHYSICALREADY, physical_ready, 0);

    // Request EV_TIMER1 in 1 sec, ignore return value

    //  CNET_enable_application(ALLNODES);
}
