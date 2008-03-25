

#include "ace/ACE.h"
#include "ace/OS.h"
#include "ace/SOCK_Dgram.h"
#include "ace/SOCK_Stream.h"
#include "ace/SOCK_Acceptor.h"
#include "ace/INET_Addr.h"

#define DATA_BUFFER_SIZE 1024
#define SIZE_DATA 19

class Server {

public:
    Server (int local_port)
        :_local_addr (local_port), _local (_local_addr), _acceptor (_local_addr)
    {
        data_buf = new char[DATA_BUFFER_SIZE];
    }

    int accept_data () {

        printf ("Starting server at port %d\n", _local_addr.get_port_number());
        int i=0;

        int byte_count=0;
        while (1)
        {
            ACE_Time_Value timeout (ACE_DEFAULT_TIMEOUT);
            if (_acceptor.accept (_stream, &_remote_addr, &timeout) == -1)
            {
                printf ("didn't get anything from TCP...count %d\n", ++i); 
            }
            else
            {
                printf ("got a connection from %s:%d\n", _remote_addr.get_host_name(), _remote_addr.get_port_number());
            }

            if ((byte_count = _local.recv (data_buf, SIZE_DATA, _remote_addr)) != -1)
            {
                data_buf[byte_count] = 0;
                printf ("data received from remote %s was '%s' \n", _remote_addr.get_host_name (), data_buf);                
            }

            ACE_OS::sleep (1);
        }

        return -1;  
    }


private:
    char *data_buf;
    ACE_INET_Addr   _remote_addr;
    ACE_INET_Addr   _local_addr;
    ACE_SOCK_Dgram  _local;
    ACE_SOCK_Stream _stream;
    ACE_SOCK_Acceptor _acceptor;
};

int main (int argc, char *argv[])
{
    if (argc < 2)
    {
        printf ("usage: %s [port number]\n", argv[0]);
        ACE_OS::exit (1);
    }
    Server server (ACE_OS::atoi (argv[1]));
    server.accept_data ();

    return 0;
}