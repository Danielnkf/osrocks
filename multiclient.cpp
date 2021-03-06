#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "mpi.h"
#include <string>
#include <iostream>
#include <iomanip>

using namespace std;

void *get_in_addr(struct sockaddr *sa);
void nsleep(int milisec);

int main(int argc, char *argv[])
{

	int numtasks, rank;
	char send_file[50];
	char recv_file[50];
	string input;
	string report;
	double start_time, end_time;
	const char PORT[]="3490"; // the port client will be connecting to
	const int MAXDATASIZE=4096; // max number of bytes we can get at once
    int sockfd, numbytes;
    char buf[MAXDATASIZE];
    int sent_total=0;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char s[INET6_ADDRSTRLEN];


	MPI_Init(&argc,&argv);
	MPI_Comm_size(MPI_COMM_WORLD, &numtasks);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);

	nsleep(rank*100);
	cout<<"Client no."<<rank+1<<": process created"<<endl;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(argv[1], PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and connect to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("client: connect");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "client: failed to connect\n");
        return 2;
    }

    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
            s, sizeof s);
    cout<<"Client no."<<rank+1<<": connected to "<<s<<endl;

    freeaddrinfo(servinfo); // all done with this structure


    //locate the file to be sent

	if(numtasks>1){
		sprintf (send_file, "harrypotter/ch%d.txt", rank+1);
		sprintf (recv_file, "harrypotter/out/ch%d.txt", rank+1);
	}else
	{
		cout<<"Enter your file name:"<<endl;
		cin>>send_file;
		sprintf (recv_file, "out.txt");
	}
	start_time = MPI_Wtime();

	cout<<"Client no."<<rank+1<<": reading from file "<<send_file<<endl;

    if (argc < 2) {
        fprintf(stderr,"usage: client hostname\n");
        exit(1);
    }

    //read file to input
	FILE *file;
	file = fopen(send_file, "r");
	if (file == NULL)
		cout<<"Client no."<<rank+1<<": file not found!"<<endl;

	while(1)
	{
		int bytes_read = fread(buf,1,sizeof(buf),file);
		if (bytes_read == 0)
			break;
		if (bytes_read < 0)
			perror("error reading from file");
		input+=buf;
	}

	input+="$END$";

	//sent input
	while (input.length()>0){
		bzero(buf,MAXDATASIZE);

		string tmp=input.substr(0,(input.length()>MAXDATASIZE)?MAXDATASIZE:input.length());
		input.erase(0,tmp.length());//reduce the report to unsent part
		strcpy(buf,tmp.c_str());

		int bytes_sent = send(sockfd,buf,tmp.length(),0);
		sent_total+=bytes_sent;
		if (bytes_sent <= 0)
		{
			perror("ERROR writing to socket");
			exit(0);
		}
		nsleep(1);
	}
	cout<<"Client no."<<rank+1<<": "<<sent_total<<" bytes have been sent. Waiting for server response..."<<endl;

    //read server response
	FILE *file1;
	file1 = fopen(recv_file, "w+");
	while(1){
		bzero(buf,sizeof(buf));
		int bytes_receive = recv(sockfd,buf,sizeof(buf),0);

		if (bytes_receive == 0)
		{
			break;
		}else if (bytes_receive < 0)
		{
			perror("ERROR: receive failed in thread.");
			close(sockfd);
			exit(1);
		}

		int bytes_write = fwrite(buf, sizeof(char), bytes_receive, file1);
	    if(bytes_write < bytes_receive)
		{
	       perror("File write failed.\n");
	    }

		if(bytes_receive<sizeof(buf)) break; //stop looping after recv done
	}

	cout<<"Client no."<<rank+1<<": report received and saved to "<<recv_file<<endl;

	//

    close(sockfd);
    end_time   = MPI_Wtime();
	cout<<"Client no."<<rank+1<<": task finished. Time usage of this task: "
			<<end_time-start_time<<"s."<<endl;

    MPI_Finalize();



    return 0;
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

//sleep for some time
void nsleep(int milisec)
{
    struct timespec delay;
    delay.tv_sec = 0;
    delay.tv_nsec =milisec * 1000000L;

    if (nanosleep(&delay, NULL)) {
	  perror("nanosleep");
    }

}
