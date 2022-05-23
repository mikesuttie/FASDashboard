
// Adding headers to http_response to allow cross-origin scripting.
// refer to:
// https://developer.mozilla.org/en-US/docs/Web/HTTP/CORS
// https://stackoverflow.com/questions/38898776/add-access-control-allow-origin-on-http-listener-in-c-rest-sdk
#define CORS_PERMISSIONS U("*") // TODO: make runtime configurable if REST API meant to be private
// Rather dirty, but required until a way is found to intercept http_responses for adding a CORS-header.
// Once a better solution has been found, erase the following macro and substitute all occurrences of 'message_reply' in the codebase with 'message.reply'
#define message_reply(___STATUS_CODE___, ___MESSAGE_BODY___) web::http::http_response response(___STATUS_CODE___);response.headers().add(U("Access-Control-Allow-Origin"), CORS_PERMISSIONS);response.set_body(___MESSAGE_BODY___);message.reply(response);
