// g++ -std=c++11 -Wall *.cpp -o SSRCON
#include <fcntl.h>
#include <err.h>
#include <errno.h>
#include <signal.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <iostream>
#include <string>

#include "SSRCON.hpp"
#include "Logger.hpp"

#define VERSION "1.00"

// Local function prototypes
bool sendRCONMessage (std::string msg_body, int32_t msg_id, int32_t msg_type);
bool readRCONMessage (int32_t expected_id, int32_t expected_type);
void signalHandler (int signum);

// Global Varible
uint8_t debug_level = DEBUG_NONE;
Logger *logger;
volatile sig_atomic_t closing_process = 0;
volatile sig_atomic_t close_reason = 0;

// Used for normal twitch IRC
uint8_t rcon_task = 0;
int rcon_return;
int rcon_sock = -1;
int rcon_port = DEFAULT_RCON_PORT;
struct sockaddr_in rcon_serv_addr;
hostent *rcon_server;
int32_t rcon_id = 0;

int main(int argc, char **argv)
{
	// Setup the logger and log the start of the process
	logger = new Logger ("SSRCON.log");
	logger->setLinePrefix ("SSRCON");
	logger->log (": Started version " VERSION " compiled on " __DATE__ ", " __TIME__ ".\n");

	// All close signals should be send to the signalHandler method
	signal (SIGTERM, &signalHandler);
	signal (SIGQUIT, &signalHandler);
	signal (SIGINT, &signalHandler);
	// Sends some program error signals to the signalHandler method
	signal (SIGILL, &signalHandler);
	signal (SIGSEGV, &signalHandler);
	signal (SIGBUS, &signalHandler);
	// All SIGCHLD signals are ignored, otherwise the child process becomes a zombie when closed
	signal (SIGCHLD, SIG_IGN);
	// ALL SIGPIPE signals are ignored, otherwise the program exits if it tries to write after the connection has dropped
	signal (SIGPIPE, SIG_IGN);

	// Processes command line arguments
	int arg_count = 1;
	for (arg_count = 1; arg_count < argc; arg_count++)
	{
		if (strcmp(argv[arg_count], "-d") == 0)
		{
			// Check to make a debug number was set
			if (argc - 1 >= arg_count + 1)
			{
				debug_level = atoi (argv[arg_count+1]);
				if (debug_level <= 3)
				{
					logger->logf (": Running in debug mode %d.\n", debug_level);
				}
				else
				{
					debug_level = 0;
					logger->log (": Unknown debug mode, I'm confused.\n");
				}
				logger->setDebugLevel (debug_level);
			}
			else
			{
				logger->log (": Why did you set the debug flag without the debug value?.\n");
			}
		}
	}

	// Get the server address from the user
	printf ("RCON Server Address: ");
	std::string user_address;
	std::getline (std::cin, user_address);

	// Get the server port from the user
	printf ("RCON Server Port: ");
	std::string user_port;
	std::getline (std::cin, user_port);
	rcon_port = strtol (user_port.c_str(), NULL, 10);
	if (rcon_port < 0)
	{
		rcon_port = DEFAULT_RCON_PORT;
	}

	// Get the server password from the user
	printf ("RCON Server Password: ");
	std::string user_password;
	std::getline (std::cin, user_password);

	// Loop until the process is closed
	while (closing_process != 1)
	{
		// Handle RCON connection
		switch (rcon_task)
		{
			// Connect to the given server and port
			case (RCON_CONNECT):
			{
				// Create socket
				rcon_sock = socket(AF_INET, SOCK_STREAM, 0);
				rcon_server = gethostbyname(user_address.c_str());
				if ((rcon_sock >= 0) && (rcon_server != NULL))
				{
					// Sets up the server address
					bzero((char *) &rcon_serv_addr, sizeof(rcon_serv_addr));
					rcon_serv_addr.sin_family = AF_INET;
					bcopy((char *)rcon_server->h_addr, (char *)&rcon_serv_addr.sin_addr.s_addr, rcon_server->h_length);
					rcon_serv_addr.sin_port = htons(rcon_port);

					// Connect
					rcon_return = connect (rcon_sock, (struct sockaddr *) &rcon_serv_addr, sizeof (rcon_serv_addr));
					if (rcon_return >= 0)
					{
						rcon_task = RCON_AUTH;
						logger->log (": Connected to the RCON server.\n");
						break;
					}
					else
					{
						logger->logf (": Unable to get the server host: %s.\n", strerror(errno));
					}
				}
				else
				{
					logger->logf (": Unable to open a socket, or find the server: %s.\n", strerror(errno));
				}
			}
			break;

			// Authorise with the server (temporally fudged)
			case (RCON_AUTH):
			{
				logger->log (": Hacking auth for the time being.\n");
				sendRCONMessage (user_password.c_str(), 0x12131415, SERVERDATA_AUTH);
				readRCONMessage (0x12131415, SERVERDATA_AUTH_RESPONSE);
				rcon_task = RCON_RUNNING;
			}
			break;

			// Connected and authorised, wait for command from user
			case (RCON_RUNNING):
			{
				std::string command;
				std::getline (std::cin, command);

				// Send command to RCON
				rcon_id++;
				logger->logf (": Sending: %s\n", command.c_str());
				sendRCONMessage (command, rcon_id, SERVERDATA_EXECCOMMAND);

				// Get responce
				readRCONMessage (rcon_id, SERVERDATA_RESPONSE_VALUE);
			}
			break;

			// Close the server
			case (RCON_CLOSE):
			{
				close (rcon_sock);
				rcon_sock = -1;

				rcon_task = RCON_CONNECT;
				sleep (10);
			}
			break;
		}

		usleep (100000);
	}

	// Find out why we are closing
	switch (close_reason)
	{
		case SIGTERM:
		case SIGQUIT:
		case SIGINT:
		{
			logger->log (": Close signal received, closing.\n");
		}
		break;
		case SIGILL:
		{
			logger->log (": Illegal instruction, closing.\n");
		}
		break;
		case SIGSEGV:
		{
			logger->log (": Read outside of allocated memory, closing.\n");
		}
		break;
		case SIGBUS:
		{
			logger->log (": Derefernced an invalid pointer, uninitalized variable or null pointer referenced, closing.\n");
		}
		break;
	}

	// Close the socket
	if (rcon_sock != -1)
	{
		close (rcon_sock);
		rcon_sock = -1;
	}

	logger->log (": Exited.\n");
	delete logger;

	return 0;
}

// Send an RCON Message
bool sendRCONMessage (std::string msg_body, int32_t msg_id, int32_t msg_type)
{
	int return_value;

	// Prepares the message
	uint8_t *msg;
	uint32_t msg_size = msg_body.size() + 14;
	int32_t msg_body_size = msg_size - 4;
	msg = (uint8_t*) malloc (msg_size * sizeof (uint8_t));
	memset (msg, 0, msg_size * sizeof (uint8_t));

	// Adds the message size
	memcpy (&msg[0], &msg_body_size, sizeof (int32_t));

	// Adds the message id
	memcpy (&msg[4], &msg_id, sizeof (int32_t));

	// Adds the message type
	memcpy (&msg[8], &msg_type, sizeof (int32_t));

	// Adds the message body
	memcpy (&msg[12], msg_body.c_str(), msg_body.size() * sizeof (uint8_t));

	// Adds the message body
	msg[msg_size-1] = 0x00;

	// Spit out the message
	logger->debug (DEBUG_DETAILED, ": Sending: ");
	if (debug_level >= DEBUG_DETAILED)
	{
		for (uint32_t t = 0; t < msg_size-1; t++)
		{
			logger->logx (msg[t], false);
		}
		logger->logx (msg[msg_size-1], true);
	}

	// Sends the message and makes sure it didn't fail
	return_value = write (rcon_sock, msg, msg_size);
	if (return_value < 0)
	{
		logger->logf (": Unable to send the following message to the IRC server: %s, reason: %s.\n", msg_body.c_str(), strerror(errno));
		rcon_task = RCON_CLOSE;
		return false;
	}
	else
	{
		// TODO: Disable this debug message
		logger->debugf (DEBUG_MINIMAL, ": Message sent successfully.\n");
	}

	free (msg);
	msg = NULL;
	return true;
}

// Reads from the socket and checks the returned values
bool readRCONMessage (int32_t expected_id, int32_t expected_type)
{
	// Creating receive buffers
	char rcon_size[4];
	memset (rcon_size, 0, 4);
	char rcon_recv[MAXDATAREAD];
	memset (rcon_recv, 0, MAXDATAREAD);

	// Read the message size
	logger->debug (DEBUG_STANDARD, ": About to read size.\n");
	rcon_return = read(rcon_sock, rcon_size, 4);
	if (rcon_return > 0)
	{
		int32_t data_size = 0;
		memcpy (&data_size, rcon_size, sizeof (int32_t));

		// Read the rest of the message
		logger->debugf (DEBUG_STANDARD, ": Size %d.\n", data_size);
		rcon_return = read(rcon_sock, rcon_recv, data_size);
		if (rcon_return > 0)
		{
			logger->debug (DEBUG_STANDARD, ": Reading message.\n");

			// Pull the message id
			int32_t msg_id = 0;
			memcpy (&msg_id, rcon_recv, sizeof (int32_t));

			// Read the message type
			int32_t msg_type = 0;
			memcpy (&msg_type, &rcon_recv[4], sizeof (int32_t));

			// Read the message body
			std::string msg_body (&rcon_recv[8], data_size - 9);

			// Check message is correct
			if (expected_id == msg_id)
			{
				logger->debug (DEBUG_MINIMAL, ": ID OK.\n");
			}
			else
			{
				logger->log (": Reply ID did not match original message.\n");
			}
			if (expected_type == msg_type)
			{
				logger->debug (DEBUG_MINIMAL, ": Type OK.\n");
			}
			else
			{
				logger->log (": Reply message type did not match expected type.\n");
			}
			if ((rcon_recv[data_size-2] == 0x00) && (rcon_recv[data_size-1] == 0x00))
			{
				logger->debug (DEBUG_MINIMAL, ": Empty String OK.\n");
			}
			else
			{
				logger->log (": Reply is missing ether the null terminator on the string, or the empty string at the end of the message.\n");
			}

			logger->logf (": Received: %s\n", msg_body.c_str());

			// Spit out the message
			logger->debug (DEBUG_DETAILED, ": Received: ");
			if (debug_level >= DEBUG_DETAILED)
			{
				for (uint32_t t = 0; t < 4; t++)
				{
					logger->logx (rcon_size[t], false);
				}
				for (int32_t t = 0; t < data_size-1; t++)
				{
					logger->logx (rcon_recv[t], false);
				}
				logger->logx (rcon_recv[data_size-1], true);
			}
		}
		else if ((rcon_return != -EAGAIN) && (rcon_return != -EWOULDBLOCK) && (rcon_return != -1) && (rcon_return != 0))
		{
			// Failed to read the message
			logger->logf (": Error on RCON socket while reading message data: %s.\n", strerror(errno));
			rcon_task = RCON_CLOSE;
			return false;
		}
	}
	else if ((rcon_return != -EAGAIN) && (rcon_return != -EWOULDBLOCK) && (rcon_return != -1) && (rcon_return != 0))
	{
		// Failed to read the message
		logger->logf (": Error on RCON socket while reading message size: %s.\n", strerror(errno));
		rcon_task = RCON_CLOSE;
		return false;
	}

	return true;
}

// Hangles the SIGTERM signal, to safely close the program down
void signalHandler (int signum)
{
	switch (signum)
	{
		case SIGTERM:
		case SIGQUIT:
		case SIGINT:
		case SIGILL:
		case SIGSEGV:
		case SIGBUS:
		{
			// Stops the process from trying to close twice
			if (!closing_process)
			{
				close_reason = signum;
				closing_process = 1;
			}
		}
		break;
	}
}