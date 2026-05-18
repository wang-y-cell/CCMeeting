#include "network/netheader.h"

QUEUE_DATA<MESG> queue_send; //发送给服务端的信息
QUEUE_DATA<MESG> queue_recv; //接收到服务端的信息
QUEUE_DATA<MESG> audio_recv; //接收到服务端的音频信息
