

#include "ace/OS.h"
#include "ace/SOCK_Dgram.h"
#include "ace/INET_Addr.h"

#define DATA_BUFFER_SIZE 1024
#define SIZE_DATA 28

class Client {
public:

    Client (const char *remote_host) 
        :_remote_addr (remote_host), _local_addr ((u_short)0), _local (_local_addr)
    {
        data_buf = new char[DATA_BUFFER_SIZE];
    }

    int send_data () 
    {
        printf ("preparing to send data to server %s:%d\n", _remote_addr.get_host_name (), _remote_addr.get_port_number());
        sprintf (data_buf, "hello from client");

        while (_local.send (data_buf, ACE_OS::strlen (data_buf), _remote_addr) != -1) 
        {
            ACE_OS::sleep (1);
        }
        return -1;
    }
    

private:
    char *data_buf;
    ACE_INET_Addr _remote_addr;
    ACE_INET_Addr _local_addr;
    ACE_SOCK_Dgram _local;
};

int main (int argc, char *argv[])
{
    if (argc < 2)
    {
        printf ("usage: %s <host:port>\n", argv[0]);
        ACE_OS::exit (1);
    }
    Client client (argv[1]);
    client.send_data ();

    return 0;
}