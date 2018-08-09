#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "threadpool/ThreadPool.h"

#include <string.h>
#include <string>
#include <utility>
#include <sstream>

#define PORT 5000

using namespace std;

void respond (int sock);
string ROOT;

int main( int argc, char *argv[] ) {
  int sockfd, newsockfd, portno = PORT;
  socklen_t clilen;
  struct sockaddr_in serv_addr, cli_addr;
  clilen = sizeof(cli_addr);
  ROOT = getenv("PWD");

  /* First call to socket() function */
  sockfd = socket(AF_INET, SOCK_STREAM, 0);

  if (sockfd < 0) {
    perror("ERROR opening socket");
    exit(1);
  }

  // port reusable
  int tr = 1;
  if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &tr, sizeof(int)) == -1) {
    perror("setsockopt");
    exit(1);
  }

  /* Initialize socket structure */
  bzero((char *) &serv_addr, sizeof(serv_addr));

  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = htons(portno);

  /* TODO : Now bind the host address using bind() call.*/
  if(bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
    perror("ERROR on binding");
    exit(1);
  }

  /* TODO : listen on socket you created */
  listen(sockfd, 5);

  printf("Server is running on port %d\n", portno);

  // ThreadPool is created
  ThreadPool pool(5);

  while (1) {
    /* TODO : accept connection */
    newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);

    if(newsockfd < 0) {
        perror("ERROR on accept");
        exit(1);
    }

    // TODO : implement processing. there are three ways - single threaded, multi threaded, thread pooled
    pool.enqueue([newsockfd] {
      respond(newsockfd);
    });
  }

  return 0;
}

string get_url(string url) {
    if(url == "/") {
        return ROOT + "/index.html";
    }
    return ROOT + url;
}

string get_type(string url) {
    string type = url.substr(url.find(".") + 1);
    if(type == "html") return "text/html";
    if(type == "css") return "text/css";
    if(type == "js") return "text/javascript";
    if(type == "jpg") return "image/jpeg";
    return "ignore";
}

pair<bool,pair<long,char*>> open_file(string url) {
    long len;
    FILE *fp = fopen(url.c_str(), "rb");
    char* for_return;
    if(!fp || fseek(fp, 0, SEEK_END) == -1) {
        return make_pair(false, make_pair(0, for_return));
    }

    len = ftell(fp);
    if (len == -1) {
        return make_pair(false, make_pair(0, for_return));
    }
    rewind(fp);

    char *body = (char*) malloc(len);
    if(!body || fread(body, len, 1, fp) != 1) {
        return make_pair(false, make_pair(0, for_return));
    }

    fclose(fp);
    return make_pair(true, make_pair(len, body));
}

void write_data(int sock, const char *data, int data_len) {
    const char *data_p = data;

    while(data_len > 0) {
        int sent = send(sock, data_p, data_len, 0);
        if(sent < 0) {
            perror("ERROR something in write_data()");
            return;
        }
        else if(sent == 0) {
            printf("The client is disconnected\n");
            return;
        }
        data_p += sent;
        data_len -= sent;
    }
}

void write_string(int sock, const char *str) {
    printf("%s", str);
    write_data(sock, str, strlen(str));
}

void respond(int sock) {
  int n;
  char buffer[9999];

  bzero(buffer,9999);
  n = recv(sock,buffer,9999, 0);

  if (n < 0) {
    printf("recv() error\n");
    return;
  } else if (n == 0) {
    printf("Client disconnected unexpectedly\n");
    return;
  } else {
    // TODO : parse received message from client
    // make proper response and send it to client
    string method = strtok(buffer, " \t");
    string url = strtok(NULL, " \t");

    url = get_url(url);
    string type = get_type(url);

    printf("The URL is: %s\n", url.c_str());
    pair<bool,pair<long,char*>> tmp;
    bool ignore_the_file = (type == "ignore");
    if(!ignore_the_file) tmp = open_file(url);
    if(!tmp.first || ignore_the_file) {
        string NF_404 = "<html> <head> <title> 404 Not Found </title> </head> <body> <h1>Not Found </h1> <p> The requested URL was not found. </p> </body> </html>";
        long len = strlen(NF_404.c_str());
        stringstream ss;
        ss << "Content-length: " + to_string(len) + "\r\n\r\n";
        string clen = ss.str();
        string outp = "HTTP/1.1 404 Not Found\r\n";
        outp += "Connection: closed\r\n";
        outp += "Content-Type: text/html\r\n";
        outp += clen;
        write_string(sock, outp.c_str());
        write_data(sock, NF_404.c_str(), len);
    }
    else {
        long len = tmp.second.first;
        char* body = tmp.second.second;
        stringstream ss;
        ss << "Content-length: " + to_string(len) + "\r\n\r\n";
        string clen = ss.str();
        string outp = "HTTP/1.1 200 OK\r\n";
        outp += "Connection: closed\r\n";
        outp += "Content-Type: ";
        outp += type;
        outp += "\r\n";
        outp += clen;
        write_string(sock, outp.c_str());
        write_data(sock, body, len);
    }
  }

  shutdown(sock, SHUT_RDWR);
  close(sock);

}
