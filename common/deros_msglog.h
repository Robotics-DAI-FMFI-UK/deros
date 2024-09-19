#ifndef __DEROS_MSGLOG_H__
#define __DEROS_MSGLOG_H__

// defines internal interface used by the deros publisher to log its messages, if logging for that publisher is enabled

#define DEROS_MSGLOG_MAX_MESSAGE_LEN 100

/** intitialize deros publisher message log
 *  @param path  path (folder) where log will be created
 *  @param prefix  a string that specifies the filename prefix of the log file name 
 *  @return  integer handle for this log session (multiple can be opened simultaneously) */
int deros_msglog_init(char *path, char *prefix);

/** log a single message - if it is already formatted as a string, it is saved directly, otherwise, 
 *  both its numeric and character representations are saved into the message log file 
 *  @param tm  timestamp when the message originated
 *  @param pub_node_name  name of the node where the message originated
 *  @param address  address to which it is being published
 *  @param msg  the message contents itself (for binary messages, set isAlreadyFormatted to 0)
 *  @param msglen  especially for binary messages we need to know their length
 *  @param isAlreadyFormatted  if 1, the message is treated as a C zero-terminated string, otherwise as a binary message */
void deros_msglog_published_msg(int dbghandle, struct timeval *tm, char *pub_node_name, char *address, 
                                char *msg, int msglen, int isAlreadyFormatted);

#endif
