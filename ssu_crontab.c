#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#define SECOND_TO_MICRO 1000000
#define N 1024
#define BUFFER_SIZE 1024

void ssu_runtime(struct timeval *begin_t,struct timeval *end_t);//프로그램 수행시간 측정 함수
int check_except_time(char *argv,int ver);//입력 주기 에러 체크함수
int check_number_range(int ver,int number);//숫자 범위 체크함수
int remove_line(FILE *fp,char *argv,int k);//명령어 라인 삭제함수

int flag_r,flag_t,flag_m;
FILE *fp;
FILE *log_fp;
char r_buf[BUFFER_SIZE];

int main(int argc, char *argv[])
{
	struct timeval begin_t, end_t;
	char *fname="ssu_crontab_file";


	if(access(fname,F_OK)==0)//파일존재여부 확인
	{
		if((fp=fopen(fname,"r+"))==NULL)//존재할경우 바로 열기
		{
			fprintf(stderr,"fopen error for %s\n",fname);
			exit(1);
		}
	}
	else
	{//존재하지 않을 경우 파일 생성후 열기
		if((fp=fopen(fname,"w+"))==NULL)
		{
			fprintf(stderr,"fopen error for ssu_crontab_file\n");
			exit(1);
		}
	}
	char *fname2="ssu_crontab_log";

	/*로그파일 열기*/
	if(access(fname2,F_OK)==0)
	{
		if((log_fp=fopen(fname2,"r+"))==NULL)
		{
			fprintf(stderr,"fopen error for %s\n",fname2);
			exit(1);
		}
	}
	else
	{
		if((log_fp=fopen(fname2,"w+"))==NULL)
		{
			fprintf(stderr,"fopen error for %s\n",fname2);
			exit(1);
		}
	}	

	gettimeofday(&begin_t, NULL);//현재시간 얻기
	while(1)
	{
		char line[BUFFER_SIZE];
		char argv[N][BUFFER_SIZE]={0};
		char temp[BUFFER_SIZE];
		char temp2[BUFFER_SIZE];
		char copy[BUFFER_SIZE];
		char log_content[BUFFER_SIZE];
		char t_buf[BUFFER_SIZE];
		char t[BUFFER_SIZE]={0};
		int length=0;
		int argc=0;
	//	int number;
		int c;//option
		int k=0;//반복문에 사용
		int ret=0;
		time_t logtime;

		fseek(fp,0,0);
		while(fgets(temp,BUFFER_SIZE,fp)!=NULL){//ssu_crontab_file에서 한줄씩 읽기
			sprintf(temp2,"%d.%s",k,temp); //ssu_crontab_file에 존재하는 명령어에 인덱스 추가
			if(fputs(temp2,stdout)==EOF){
				fprintf(stderr,"standard output error\n");
				exit(1);
			}
			k++;
		}
		printf("\n");

		memset(line,0,BUFFER_SIZE);
		memset(copy,0,BUFFER_SIZE);

		printf("20182620> ");
		fgets(line,sizeof(line),stdin);//입력받은 문자열 line에 저장
		strcpy(copy,line);

		memset(argv,0,sizeof(argv));
		int j=0;
		char *ptr=strtok(line," ");//띄어쓰기 기준으로 자르기
		while(ptr!=NULL)
		{
			strcpy(argv[j],ptr);
			ptr=strtok(NULL," ");
			j++;
		}

		for(int i=0;argv[0][i]!=0;i++){	
			if(argv[0][i]=='\n'){
				argv[0][i]=0;
				break;
			}
		}
		if(!strcmp(argv[0],"add")||!strcmp(argv[0],"ADD"))
		{//명령어가 add 일 경우
			if(argv[1][0]!='\0' && argv[2][0]!='\0' && argv[3][0]!='\0' && argv[4][0]!='\0'&& argv[5][0]!='\0')
			{//빈 시간이 없다면
				for(int i=1; i<=5 ; i++)
				{//분 시 일 월 요일 순으로 예외검사
					ret=0;
					ret=check_except_time(argv[i],i);//시간 예외검사
					if(ret==0)//예외가 발생하면 멈추고 다시 프롬프트 띄우기
					{
						fprintf(stderr,"input <minute><hour><day><month><daynum>\n");
						break;
					}
				}
			}
				//시간이 맞게 들어갔다면 파일에 추가
			strcpy(log_content,copy+strlen(argv[0])+1);
			if(ret==1){
				if(fputs(log_content,fp)==EOF)//ssu_crontab_log에 log 작성
				{
					fprintf(stderr,"this line can't write to file\n");
					continue;
				}
				//add 성공시에 로그 찍기
				time(&logtime);//현재시간 얻기
				strcpy(t_buf,ctime(&logtime));
				t_buf[strlen(t_buf)-1]='\0';
				sprintf(t,"[%s] %s %s",t_buf,"add",log_content);//현재시간과 add, 입력받은 명령어를 더함

				fseek(log_fp,0,SEEK_END);
				fputs(t,log_fp);//로그 찍기
				continue;
			}
			
		}
		/*remove 옵션 수행*/
		else if(!strcmp(argv[0],"remove")||!strcmp(argv[0],"REMOVE"))
		{
			if(!isdigit(argv[1][0]))//remove 숫자 입력
			{
				fprintf(stderr,"input <remove> <number>\n");
				continue;
			}

			ret=remove_line(fp,argv[1],k);	//remove 수행
			if(ret==1)//해당 명령어 제거 완료시 로그 찍기
			{
				time(&logtime);
				strcpy(t_buf,ctime(&logtime));
				t_buf[strlen(t_buf)-1]='\0';
				sprintf(t,"[%s] %s %s",t_buf,"remove",r_buf);//현재시간+remove+제거한 명령어
				fseek(log_fp,0,SEEK_END);
				fputs(t,log_fp);//로그 찍기
			}
			continue;
		}
		/*exit 명령어일 경우*/
		else if(!strcmp(argv[0],"exit")||!strcmp(argv[0],"EXIT"))
		{
			gettimeofday(&end_t,NULL);//프로그램 종료시간 end_t에 저장
			ssu_runtime(&begin_t,&end_t);//프로그램 수행시간 측정함수
			break;//exit 입력시에만 프로그램 종료
		}
		else
		{
			fprintf(stderr,"input add, remove, exit \n");
			continue;
		}
	}

	fclose(fp);
	fclose(log_fp);


	exit(0);
}

void ssu_runtime(struct timeval *begin_t, struct timeval *end_t)
{
	end_t->tv_sec -= begin_t->tv_sec;//수행시간측정(초단위)

	if(end_t->tv_usec < begin_t->tv_usec){//마이크로 단위에서 종료시간이 시작시간보다 작을 때
		end_t->tv_sec--;//1초 빼기
		end_t->tv_usec+=SECOND_TO_MICRO;//1초를 마이크로 단위로 바꾸고 tv_usec에 더해줌
	}
	
	end_t->tv_usec -= begin_t->tv_usec;//수행시간(마이크로단위)
	printf("Runtime: %ld:%06ld(sec:usec)\n",end_t->tv_sec,end_t->tv_usec);
}

int check_except_time(char *str,int ver)
{
	int i=0;
	int number=0;
	int cnt_s=0;//'/'의 개수
	int cnt_b=0;//'-'의 개수
	int is_num=0,is_star=0,is_slash=0,is_bar=0,is_comma=0;//바로 앞 문자종류
	int is_range=0;//앞에 범위가 나왔을경우

	while(str[i]!='\0')
	{
		if(str[i]=='*'){//'*'인 경우
			if(is_star==1||is_num==1||is_slash==1||is_bar==1)
				return 0;
			//앞에 콤마이거나 처음일때 가능

			//앞에 있는 문자에 대해 0 처리
			is_comma=0;

			is_star=1;
			is_range=1;
			i++;
			continue;
		}
		else if(str[i]=='-'){//'-'인 경우
			if(is_range==1||is_star==1||is_slash==1||is_bar==1||is_comma==1)
				return 0;
			if(is_num==1)//앞에 숫자가 반드시 있어야함
			{
				is_num=0;
				is_bar=1;
				is_range=1;//범위를 나타냄
				i++;
				continue;
			}
			else
				return 0;
		}
		else if(str[i]=='/'){
			if(is_range==0)
				return 0;
			else{
				i++;
				/*바로 앞에 있던 문자 0처리*/
				is_star=0,is_num=0;
				
				is_slash=1;
				is_range=0;
				continue;
			}
		}
		else if(str[i]==','){
			if(is_slash==1||is_comma==1||is_bar==1)
				return 0;

			is_range=0;

			//앞에 있던 문자에 대해 0처리
			is_star=0,is_num=0;
			number=0;

			i++;
			continue;
		}
		else if(isdigit(str[i])!=0){
			if(is_star==1||is_num==1)
				return 0;

			if(is_bar==1)
			{
				if(number>atoi(str+i))//앞의 수보다 작으면 에러
					return 0;
			}

			number=atoi(str+i);
			if(check_number_range(ver,number)==0)
				return 0;

			is_num=1;
			
			//앞에 있는 문자에 대해 0 처리
			is_slash=0,is_bar=0,is_comma=0;

			if(number>=10)//숫자가 두자리 수 일 경우
			{
				i=i+2;
				continue;
			}
			else//숫자가 한자리 수일 경우
			{
				i++;
				continue;
			}
		}
		
		else
		{
			return 0;
		}
	}
	return 1;
}
int check_number_range(int ver,int number)
{//분 시 일 월 요일에 대해 범위 측정
	if(ver==1)//0~59분
	{
		if(0<=number&&number<=59)
			return 1;
	}
	else if(ver==2){//0~23시
		if(0<=number&&number<=23)
			return 1;
	}
	else if(ver==3){//1~31일
		if(1<=number&&number<=31)
			return 1;
	}
	else if(ver==4){//1월~12월
		if(1<=number&&number<=12)
			return 1;
	}
	else if(ver==5){//일~토(0~6)
		if(0<=number&&number<=6)
			return 1;
	}

	return 0;
}
int remove_line(FILE *fptr,char *argv,int k){//입력받은 라인삭제하는 함수

	memset(r_buf,0,BUFFER_SIZE);
	//명령어 'remove' 역할
	int num;//몇번째줄
	sscanf(argv,"%d",&num);

	if(k<=num)
	{
		fprintf(stderr,"input lower number\n");
		return 0;
	}

	int count=0;
	FILE *fp2;//임의의 파일포인터
	if((fp2=fopen("new_file","w"))==NULL)//삭제되지 않을 부분 저장하는 파일
	{
		fprintf(stderr,"fopen error for new_file\n");
		return 0;
	}

	char temp[BUFFER_SIZE];//한줄씩 받아서 임의로 담아둠
	memset(temp,0,BUFFER_SIZE);
	fseek(fptr,0,0);
	while(fgets(temp,BUFFER_SIZE,fptr)!=NULL)
	{
		if(num==count)//삭제될 부분
		{
			strcpy(r_buf,temp);
			count++;
			continue;
		}
		else//저장될 부분은 임의의 파일에 넣어둠
		{
			fwrite(temp,strlen(temp),1,fp2);
			count++;
		}
	}
	fclose(fptr);
	remove("ssu_crontab_file");//원래 파일 삭제

	if(rename("new_file","ssu_crontab_file")<0)//임의파일 이름 변경
	{
		fprintf(stderr,"rename new_file to ssu_crontab_file\n");
		return 0;
	}
	fclose(fp2);//임의의 파일 디스크립터 닫기
	if((fp=fopen("ssu_crontab_file","r+"))==NULL)//다시 fp로 파일 열어서 원래상태랑 같아짐
	{
		fprintf(stderr,"fopen error for ssu_crontab_file\n");
		return 0;
	}
	return 1;
}
