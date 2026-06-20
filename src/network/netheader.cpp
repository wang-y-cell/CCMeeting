#include "network/netheader.h"
/*发送给服务器的消息队列*/
QUEUE_DATA<MESG> queue_send; 
/*接收来自服务器的消息队列*/
QUEUE_DATA<MESG> queue_recv; 
/*接收来自服务器的音频消息队列*/
QUEUE_DATA<MESG> audio_recv; 
