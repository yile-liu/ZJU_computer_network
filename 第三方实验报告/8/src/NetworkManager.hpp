#ifndef NETWORKMANAGER_HPP
#define NETWORKMANAGER_HPP

class NetworkManager
{
/* Use the following interface to manage connection */
public:
	NetworkManager();
	void Listen(int port);
	int Accept();
	void Close();

/* The two file descriptor is private */
private:
	int listen_fd, conn_fd;
};

#endif
