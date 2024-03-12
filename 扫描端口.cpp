#include<WinSock2.h> 
#include<iostream>
#include<time.h>
#include<stdlib.h>
#include<thread>
#include<ctype.h>
#include<mutex>
#include<map>
#pragma comment(lib,"Ws2_32")
using namespace std;
int port_g;
struct node {
	int  port_start;
	int port_end;
	string start_ip_addr;
	string end_ip_addr;
};

//�������
string port_server(int port) {
    string temp = "unknow";
    map<int, string> server_map;
    server_map[21] = "FTP";
    server_map[22] = "SSH";
    server_map[23] = "Telnet";
    server_map[25] = "SMTP";
    server_map[53] = "DNS";
    server_map[69] = "TFTP";//���ļ�����Э��
    server_map[80] = "HTTP";
    server_map[109] = "POP2";//POP2��Post Office Protocol Version 2���ʾ�Э��2
    server_map[110] = "POP3";//�ʼ�Э��3
    server_map[443] = "HTTPS";
    server_map[3306] = "MYSQL";
    server_map[8080] = "WWW����";
    if (server_map.find(port) != server_map.end()) {
        return server_map.find(port)->second;
    }
    else {
        return temp;
    }
}


//�˿�ɨ��
void scan(node N, mutex& port_lock) {
	int port_temp;
    string server;
	SOCKET s;
	while (1) {
		if (port_g > N.port_end) {
			break;
		}
		port_lock.lock();
		port_temp = port_g;
		port_g++;
		port_lock.unlock();
		s = socket(AF_INET, SOCK_STREAM, 0);
		if (s == INVALID_SOCKET) {
			cout << "�׽��ִ���ʧ��" << endl;
			return;
		}
		sockaddr_in dest_com;
		dest_com.sin_family = AF_INET;
		dest_com.sin_port = htons(port_temp);
		dest_com.sin_addr.S_un.S_addr = inet_addr(N.start_ip_addr.c_str());
		int result = connect(s, (sockaddr*)&dest_com, sizeof(sockaddr_in));
		if (result == 0) {
			//cout << "��⵽Ŀ����������" << port_temp << "�˿�" << endl;//cout���������
            server = port_server(port_temp);
			printf("��⵽Ŀ����������%d�˿�   %s\n", port_temp,server.c_str());
		}
		closesocket(s);
	}
	return;
}

//PING
// IP���ݰ�ͷ�ṹ
typedef struct iphdr
{
    unsigned int headLen : 4;//λ��headlenռ��һ�ֽڵ���λ
    unsigned int version : 4;//version ռ��һ�ֽڵĺ���λ
    unsigned char tos;//8λ��������TOS
    unsigned short totalLen;//16λ�ܳ���
    unsigned short ident;//16λ��ʶ
    unsigned short fragAndFlags;//��־λ
    unsigned char ttl;//����ʱ��
    unsigned char proto;//8λЭ��
    unsigned short checkSum;//16λУ���
    unsigned int sourceIP;//32λԴIP��ַ
    unsigned int destIP;//32λĿ��IP��ַ
}IpHeader;

// ICMP����ͷ�ṹ
typedef struct ihdr
{
    unsigned char iType;
    unsigned char iCode;
    unsigned short iCheckSum;
    unsigned short iID;
    unsigned short iSeq;
    unsigned long  timeStamp;
}IcmpHeader;

// ����ICMP����У���(����ǰҪ��)
unsigned short checkSum(unsigned short* buffer, int size)
{
    unsigned long ckSum = 0;

    while (size > 1)
    {
        ckSum += *buffer++;
        size -= sizeof(unsigned short);
    }

    if (size)
    {
        ckSum += *(unsigned char*)buffer;
    }

    ckSum = (ckSum >> 16) + (ckSum & 0xffff);
    ckSum += (ckSum >> 16);

    return unsigned short(~ckSum);
}

// ���ICMP������ľ������
void fillIcmpData(char* icmpData, int dataSize)
{
    IcmpHeader* icmpHead = (IcmpHeader*)icmpData;
    icmpHead->iType = 8;  // 8��ʾ�����
    icmpHead->iCode = 0;
    icmpHead->iID = (unsigned short)GetCurrentThreadId();
    icmpHead->iSeq = 0;
    icmpHead->timeStamp = GetTickCount();
    char* datapart = icmpData + sizeof(IcmpHeader);
    memset(datapart, 'x', dataSize - sizeof(IcmpHeader)); // ���ݲ���Ϊxxx..., ʵ������32��x
    icmpHead->iCheckSum = checkSum((unsigned short*)icmpData, dataSize); // ǧ��Ҫע�⣬���һ��Ҫ�ŵ����
}

// �Է��ص�IP���ݰ����н�������λ��ICMP����
int decodeResponse(char* buf, int bytes, struct sockaddr_in* from, int tid)
{
    IpHeader* ipHead = (IpHeader*)buf;
    unsigned short ipHeadLen = ipHead->headLen * 4;
    if (bytes < ipHeadLen + 8) // ICMP���ݲ�����, ���߲�����ICMP����
    {
        return -1;
    }

    IcmpHeader* icmpHead = (IcmpHeader*)(buf + ipHeadLen);  // ��λ��ICMP��ͷ����ʼλ��
    if (icmpHead->iType != 0)   // 0��ʾ��Ӧ��
    {
        return -2;
    }

    if (icmpHead->iID != (unsigned short)tid) // ��Ӧ���
    {
        return -3;
    }

    int time = GetTickCount() - (icmpHead->timeStamp); // ����ʱ���뷢��ʱ��Ĳ�ֵ
    if (time >= 0)
    {
        return time;
    }

    return -4; // ʱ�����
}

// ping����
int ping(const char* ip, unsigned int timeout)
{
    // �����ʼ��
    WSADATA wsaData;
    WSAStartup(MAKEWORD(1, 1), &wsaData);
    unsigned int sockRaw = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);  // icmp
    setsockopt(sockRaw, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));  // �����׽��ֵĽ��ճ�ʱѡ��
    setsockopt(sockRaw, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof(timeout));  // �����׽��ֵķ��ͳ�ʱѡ��

    // ׼��Ҫ���͵�����
    int  dataSize = sizeof(IcmpHeader) + 32; // ���ݹ�32��x
    char icmpData[1024] = { 0 };
    fillIcmpData(icmpData, dataSize);
    unsigned long startTime = ((IcmpHeader*)icmpData)->timeStamp;

    // Զ��ͨ�Ŷ�
    struct sockaddr_in dest;
    memset(&dest, 0, sizeof(dest));
    struct hostent* hp = gethostbyname(ip);
    memcpy(&(dest.sin_addr), hp->h_addr, hp->h_length);
    dest.sin_family = hp->h_addrtype;

    // ��������
    sendto(sockRaw, icmpData, dataSize, 0, (struct sockaddr*)&dest, sizeof(dest));


    int iRet = -1;
    struct sockaddr_in from;
    int fromLen = sizeof(from);
    while (1)
    {
        // ��������
        char recvBuf[1024] = { 0 };
        int iRecv = recvfrom(sockRaw, recvBuf, 1024, 0, (struct sockaddr*)&from, &fromLen);
        int time = decodeResponse(recvBuf, iRecv, &from, GetCurrentThreadId());
        if (time >= 0)
        {
            iRet = 0;   // alive
            break;
        }
        else if (GetTickCount() - startTime >= timeout || GetTickCount() < startTime)
        {
            iRet = -1;  // ping��ʱ
            break;
        }
    }

    // �ͷ�
    closesocket(sockRaw);
    WSACleanup();

    return iRet;
}

//IP���ɨ��
void ip_addf_scan(string start_ip_addr,string end_ip_addr) {
    string ip_all[255];
    unsigned start_ip, end_ip, index, i = 0;
    start_ip = htonl(inet_addr(start_ip_addr.c_str()));
    index = start_ip;
    end_ip = htonl(inet_addr(end_ip_addr.c_str()));
    struct in_addr temp;//����ת��
    for (index = start_ip;index <= end_ip;index++) {
        temp.S_un.S_addr = ntohl(index);
        ip_all[i] = inet_ntoa(temp);//inet_ntoaת�������char*��
        i++;
    }
    for (int j = 0;j < i;j++) {
        int p_result = ping(ip_all[j].c_str(), 3000);
        if (p_result == 0) {
            cout << ip_all[j] << " Ŀ�������ѿ���" << endl;
        }
    }
}


int main() {
	node N;
	WSADATA wsa;
	WSAStartup(MAKEWORD(2, 2), &wsa);
	int thread_count;
	mutex port_lock;//������
	thread* mythread = new thread[2000];//�߳�����
	time_t start_time, end_time;
	int function;
    double time_pay;


	cout << "**********************"<<endl;
	cout << "1��ȫ���Ӷ˿�ɨ��" << endl;
    cout << "2��ɨ��IP���" << endl;
	cout << "0���˳�" << endl;
    cout << "**********************" << endl;
key:
    cout << "ѡ����" << endl;
	cin >> function;
	switch (function)
	{
	case 1:
		cout << "����Ҫɨ���IP��ַ" << endl;
		cin >> N.start_ip_addr;
		cout << "����ɨ��Ķ˿ڷ�Χ" << endl;
		cin >> N.port_start >> N.port_end;
		cout << "����Ҫ�������߳��������1500��" << endl;
		cin >> thread_count;
		port_g = N.port_start;
		start_time = clock();
		for (int i = 0;i < thread_count;i++) {
			mythread[i] = thread(scan, N, ref(port_lock));
		}
		for (int i = 0;i < thread_count;i++) {
			mythread[i].join();
		}
		delete[]mythread;
		end_time = clock();
		time_pay = double(end_time - start_time) / CLOCKS_PER_SEC;
		cout << "����ʱ��" << time_pay << "s"<<endl;
		goto key;
    case 2:
        cout<<"��������ɨ�������(��ʼIP~��ֹIP)"<<endl;
        cin >> N.start_ip_addr >> N.end_ip_addr;
        ip_addf_scan(N.start_ip_addr,N.end_ip_addr);
        goto key;
    
	case 0:
		break;
	}
	return 0;
}