#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/time.h>

#include <windows.h>

#include <openssl/ec.h>
#include <openssl/ecdsa.h>
#include <openssl/obj_mac.h>


unsigned char *text2bin(unsigned int *le, char *s) {
	char tmp[3] = "00";
	int v, i, len = strlen(s)/2;
	unsigned char *res = malloc(len);
	for (i=0; i<len; i++) {
		tmp[0] = s[2*i];
		tmp[1] = s[2*i+1];
		if (sscanf(tmp, "%x", &v)!=1 || v<0 || v>255) {
			printf("Not a hex string: %s\n", s);
			exit(1);
		}
		res[i] = (unsigned char)v;
	}
	*le = len;
	return res;
}

static int verify(unsigned char *pkey, unsigned int pkl,
	unsigned char *sign, unsigned int sil, unsigned char *hasz) {
	EC_KEY* ecpkey = EC_KEY_new_by_curve_name(NID_secp256k1);
	if (!ecpkey) {
		printf("EC_KEY_new_by_curve_name error!\n");
		return -1;
	}
	if (!o2i_ECPublicKey(&ecpkey, (const unsigned char **)&pkey, pkl)) {
		printf("o2i_ECPublicKey fail!\n");
		return -2;
	}
	int res = ECDSA_verify(0, hasz, 32, sign, sil, ecpkey);
	EC_KEY_free(ecpkey);
	return res;
}

int readall(SOCKET sock, unsigned char *p, int l) {
	int i, lensofar = 0;
	while (lensofar<l) {
		i = recv(sock, p+lensofar, l-lensofar, 0);
		if (i<=0) {
			return -1;
		}
		lensofar+= i;
	}
	return 0;
}


DWORD WINAPI one_server(LPVOID par) {
	unsigned char pkey[256], sign[256], hasz[32];
	unsigned char c, pkl, sil;
	SOCKET sock;

	memcpy(&sock, par, sizeof sock);
	free(par);

	if (readall(sock, &c, 1)) {
		goto err;
	}
	//printf("Got cmd 0x%02x\n", c);
	if (c==1) { // ECDSA verify
		// PublicKey
		if (readall(sock, &pkl, 1)) {
			goto err;
		}
		if (readall(sock, pkey, pkl)) {
			goto err;
		}
		if (readall(sock, &sil, 1)) {
			goto err;
		}
		if (readall(sock, sign, sil)) {
			goto err;
		}
		if (readall(sock, hasz, 32)) {
			goto err;
		}
		c = (unsigned char)verify(pkey, pkl, sign, sil, hasz);
		send(sock, &c, 1, 0);
	}

err:
	//printf("Closing socket %d\n", sock);
	closesocket(sock);
}



int main( int argc, char **argv )
{
#ifdef WINDOWS
	WSADATA wsdata;
	WSAStartup(MAKEWORD(2, 2), &wsdata);
#endif

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(16667);
	addr.sin_addr.s_addr = inet_addr("127.0.0.1");

	SOCKET sock = socket( AF_INET, SOCK_STREAM, 0 );
	if (sock==-1) {
		fprintf( stderr, "Cannot create socket\n" );
		return 1;
	}
	if (bind(sock, (struct sockaddr*)&addr, sizeof(addr))==-1) {
		fprintf(stderr, "Cannot bind do specified port\n");
		return 1;
	}

	listen(sock, 5);
	printf("TCP server ready\n");
	while (1) {
		int len = sizeof addr;
		memset(&addr, 0, len);
		SOCKET clnt = accept(sock, (struct sockaddr*)&addr, &len);
		if (clnt==-1) {
			fprintf( stderr, "Cannot accept connection\n" );
			continue;
		}
		//printf("Socket %d connected from %s\n", clnt, inet_ntoa(addr.sin_addr));
		void *tmp = malloc(sizeof clnt);
		memcpy(tmp, &clnt, sizeof clnt);
		if (!CreateThread(NULL, 0, one_server, tmp, 0, NULL)) {
			fprintf( stderr, "Cannot create thread\n" );
			free(tmp);
			closesocket(clnt);
		}
	}

	return 0;
}
