extern int HttpHandleTransaction(void);

extern char *HttpGetBufferStart(void);

int (*HttpGetDataCallBack)(char *buffer, int length);

/*Error handling
Four possibilities:
-	Fatal error ie can do nothing about, must bomb out and try again.
	An example is the client closing the socket unexpectedly.
	Log the error.
	Set the error type to 0.
	Set the return value to FAILURE (-1).
	
-	Cannot send back a response - will result in a 1xx, 3xx, 4xx, or 5xx response
	An example is that could not decipher the request
	Log the error only if debugging - normally don't need to do this
	Set the error type to eg: 500 (Internal Server Error) or 400 (Bad Request)
	Set the error message to what you want the user to see
	Set the return value to FAILURE (-1).
	
-	Cannot deal with an element in part of the response - will result in a 200 OK response
	An example is not being able to open a file in the index
	Put an explanation instead of the text that would have been displayed in that part of the page
	Leave the error type at 0.
	Set the return value to SUCCESS (0).
	
-	No problem - will result in a 200 OK response
	Leave the error type at 0.
	Set the return value to SUCCESS (0).
*/
#define ERROR_MSG_SIZE 1024
extern int  HttpErrorType;                //Only 0 (no error), 400 Bad Request, 404 Not Found, 500 Internal Server Error, or 501 Not Implemented are understood
extern char HttpErrorMsg[ERROR_MSG_SIZE]; //This will be sent as the body of a warning message to supplement the above
extern char HttpSheet[20];
