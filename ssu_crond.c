#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <dirent.h>
#include <signal.h>
#include <syslog.h>
#include <sys/stat.h>
#include <sys/types.h>

#define N 1024
#define BUFFER_SIZE 1024

int ssu_daemon_init(void);//디몬프로세스 초기화 함수
void ssu_check_crond(FILE *fp);//crond 명령을 체크해서 수행하는 함수
int ssu_check_time(char *str,int tnum);//crond명령 주기 체크 함수

FILE *log_fp;

int main(int argc, char *argv[])
{
	if(ssu_daemon_init()<0){//디몬 프로세스 초기화
		fprintf(stderr,"daemon initialization error\n");
		exit(1);
	}

	exit(0);

}
int ssu_daemon_init(void)
{
	pid_t pid;
	int fd, maxfd;
	FILE *c_fp;
	char *cron_fname="ssu_crontab_file";
	
	if((pid = fork())<0)
	{
		fprintf(stderr,"fork error\n");
		exit(1);
	}

	else if(pid!=0)
	{
		exit(0);
	}

	//자식일 경우 디몬 프로세스 실행
	setsid();
	
	//시그널 무시
	signal(SIGTTIN,SIG_IGN);
	signal(SIGTTOU,SIG_IGN);
	signal(SIGTSTP,SIG_IGN);
	maxfd = getdtablesize();

	for(fd = 0; fd < maxfd; fd++)
		close(fd);

	umask(0);
	fd=open("/dev/null",O_RDWR);
	dup(0);
	dup(0);

	/*crond 진행. 중간에 에러가 떠도 멈추지 않고 무한 반복*/
	while(1)
	{
		if(access(cron_fname,F_OK)==0)//ssu_crontab_file여부 확인
		{
			if((c_fp=fopen(cron_fname,"r+"))==NULL)//이미 존재할 경우
			{
				fprintf(stderr,"fopen error for %s\n",cron_fname);
				exit(1);
			}
		}
		else
		{//존재하지 않는 경우
			if((c_fp=fopen(cron_fname,"w+"))==NULL)
			{
				fprintf(stderr,"fopen error for %s\n",cron_fname);
				exit(1);
			}
		}

		ssu_check_crond(c_fp);//ssu_check_crond 실행

		sleep(1);

		fclose(c_fp);
	}
}

void ssu_check_crond(FILE *fp)
{
	char line[BUFFER_SIZE];//ssu_crontab_file에 써있는 한줄
	char command[BUFFER_SIZE];//실 명령어 저장
	char copy[BUFFER_SIZE];
	char tbuf[BUFFER_SIZE];
	char temp[BUFFER_SIZE]; //line 복사
	long int tnum=0;//시간 저장

	memset(line,0,BUFFER_SIZE);
	memset(command,0,BUFFER_SIZE);
	memset(copy,0,BUFFER_SIZE);
	memset(tbuf,0,BUFFER_SIZE);
	memset(temp,0,BUFFER_SIZE);

	/*ssu_crontab_log 파일 열기*/
	char *log_fname="ssu_crontab_log";
	if(access(log_fname,F_OK)==0)
	{
		if((log_fp=fopen(log_fname,"r+"))==NULL)
		{
			fprintf(stderr,"fopen error for %s\n",log_fname);
			exit(1);
		}
	}
	else
	{
		if((log_fp=fopen(log_fname,"w+"))==NULL)
		{
			fprintf(stderr,"fopen error for %s\n",log_fname);
			exit(1);
		}
	}

	/*현재시간 구하기*/
	time_t t;
	time(&t);
	struct tm *tm = localtime(&t);

	if(tm->tm_sec!=0)
		return;

	fseek(fp,0,SEEK_SET);
	/*ssu_crontab_file쓰여있는 명령어를 현재 실행가능한지 확인*/
	while(fgets(line,BUFFER_SIZE,fp) != NULL)
	{//파일에 써져있는 명령어 한줄씩 읽기

		strcpy(temp,line);
		char argv[N][BUFFER_SIZE]={0};
		memset(argv,0,sizeof(argv));

		/*현재시간 argv에 저장*/
		int j=0;
		char *ptr=strtok(line," ");
		while(ptr!=NULL)
		{//argv[0]부터 분 시 일 월 년
			strcpy(argv[j],ptr);
			ptr=strtok(NULL," ");
//			printf("argv[%d]: %s\n",j,argv[j]);
			j++;
		}

		/*주기 제외한 명령어 저장*/
		strcpy(command,argv[5]);//가장 처음의 명령어 저장(ex : echo)
		for(int k=6;k<j;k++)
		{//이후의 명령어 저장
			strcat(command," ");
			strcat(command,argv[k]);
		}

//		printf("command: %s\n",command);

		int i=0;
		/*주기 측정*/
		for(i=0;i<5;i++)
		{
			switch(i){
				case 0: tnum=tm->tm_min; break;//분 부분을 저장
				case 1: tnum=tm->tm_hour; break;//시 부분 저장
				case 2: tnum=tm->tm_mday; break;//일 부분 저장
				case 3: tnum=(tm->tm_mon)+1; break;//월 부분 저장
				case 4: tnum=tm->tm_wday; break;//요일 부분 저장
			}

//			printf("%ld\n",tnum);

			if(ssu_check_time(argv[i],tnum)==0)//해당 명령어가 현재 실행가능한지 검사
				break;

		}

		if(i!=5)// 시간주기가 현재시간과 다를경우
			continue;//다른 명령어 실행을 위해 종료

		else//명령어 실행가능한 경우
		{
			int ret=0;
			ret=system(command);//명령어 실행
			
			if(ret!=0)
				continue;
			else{//명령어 실행 성공. 로그 찍기
				strcpy(tbuf,ctime(&t));
				tbuf[strlen(tbuf)-1]='\0';
				sprintf(copy,"[%s] %s %s",tbuf,"run",temp);// 명령어 실행 성공시 로그 작성
			//	printf("copy:  %s\n",copy);

				fseek(log_fp,0,SEEK_END);
				fputs(copy,log_fp);
				continue;
			}
		}
	}
	fclose(log_fp);
	return;
}

int ssu_check_time(char *str,int tnum)
{
	int number=0;//숫자 저장
	int count=0;//*제외 문자 개수
	int ret=0; //리턴값 저장
	int is_star=0;// * 유무
	int is_bar=0;// - 유무
	int is_slash=0;// / 유무
	int numArr[BUFFER_SIZE]={0,};//숫자 저장 배열
	int k=0,l=0;

//	printf("tnum: %d\n",tnum);
	while(str[k]!='\0')//문자열이 끝날때까지 반복
	{
		number=0;
		/*숫자일 경우*/
		if(isdigit(str[k])!=0){
			number=atoi(str+k);
			numArr[l]=number;
			l++;
			if(number>=10)//두자리일 경우. 문자열 앞서나감
				k=k+2;
			else//한자리일 경우
				k++;

		}

		/* '*'일 경우*/
		else if(str[k]=='*')
		{
			is_star=1;
			k++;
		}

		/* '-'일 경우*/
		else if(str[k]=='-')
		{
			is_bar=1; // - 존재
			k++;
			count++;//문자개수 1증가
		}
		
		/* '/'일 경우*/
		else if(str[k]=='/')
		{
			is_slash=1;// / 존재
			k++;
			count++;//문자개수 1증가
		}

		/* ','이 거나 다음에서 끝날 경우*/
		if(str[k]==','||str[k]=='\0')
		{
			if(count==0)//숫자로만 이루어진 경우
			{
				if(is_star==1)//매시간마다 시행하는 경우.->항상 참
					return 1;

				if(tnum==numArr[l-1])//현재시간과 주기가 같을경우
					return 1;//다를 경우
			}
			else if(count==1)//*제외 문자가 하나 존재할 경우. '-','/'만 가능
			{
				if(is_bar==1){
					if(numArr[l-2]<=tnum&&tnum<=numArr[l-1])//'-' 앞 시간과 뒤 시간 비교
						return 1;
				}

				else if(is_star==1&&is_slash==1){
					if((tnum-(numArr[l-1]-1))%numArr[l-1]==0)//현재시간이 /숫자로 나누어 떨어지지 않는 경우
						return 1;
				}
			}
			else if(count==2)//*제외 문자가 두개 존재할 경우
			{
				if((is_bar==1)&&(is_slash==1)){//'-/'인경우만 가능
					if((numArr[l-3]<=tnum) && (tnum<=numArr[l-2]))
					{
						if((tnum-numArr[l-3]-(numArr[l-1]-1)) % numArr[l-1] ==0)
							return 1;
					}
				}
			}

			/*변수 초기화*/
			count=0;
			is_star=0;
			is_bar=0;
			is_slash=0;
			ret=0;
			k++;
		}

	}
	return ret;
}
