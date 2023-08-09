#ifndef REQUESTMANAGER_HPP
#define REQUESTMANAGER_HPP

class RequestManager {
public:
	void Start();
	RequestManager(int connfd);
private:
	int connfd;
};


#endif
