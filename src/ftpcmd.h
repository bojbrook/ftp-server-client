#ifndef FTPCMP
#define FTPCMP

#define QUIT_COMMAND 				"221 Goodbye."
#define OK_COMMAND 					"200 Command OK"
#define HELP_COMMAND				"214 Help message (for human user)."
#define NO_DATA_CONNECTION	"425 Can't open data connection."
#define FILE_ERROR					"452 Error writing file."
#define COMMAND_ERROR				"500 Syntax error (unrecognized command)."
#define SYNTAX_ERROR				"501 Syntax error (invalid arguments)."
#define CMDTAG							"ftpcmd:"

void client_quit(int socket);
void client_read_command(int socket,char *print_buffer,int fsize);
void client_send_data_port(int socket, int port);
int client_ls_dir(int socket, char* args);
int check_status_code(int socket,char *buffer);

#endif