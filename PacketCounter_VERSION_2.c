#include<unistd.h>
#include<stdio.h>
#include<stdlib.h>
#include<time.h>
#include<inttypes.h>
#include<math.h>
#include<string.h>
#include<pthread.h>
//#include<windows.h>


#define IP_SIZE 32
#define IP_PARTS_SIZE 8
#define IP_PARTS 4
#define NIL -1
#define NO_OF_THREADS 2
#define NO_OF_LOCKS 2
#define NO_OF_PACKETS 1
#define max_size 250

int ipMask[IP_PARTS];
pthread_mutex_t lock[NO_OF_LOCKS+1];
unsigned long start_time;
int packet_unhandled=0;
int packet_drop=0;
int BUFFER_SIZE;
int EXPIRY_TIME;
int display_time_out;
int unique_ip;
int iteration=1;
int sleep_per_iteration=0;
int iteration_size;
int subnet_size;
int output_type;
int k;//testcase number

struct ip_host
{
	int mapped_host_value;
	int ip[IP_PARTS];
	unsigned long count;
	int next_host_index;
	unsigned long last_updated_timestamp;
};
struct cleanup_thread_parameters
{
	int lower;
	int upper;
	int *subnet;
	struct ip_host *buffer;
	int *free_buffer;
	int *current_index;
	int subnet_size;
};
struct display_thread_parameters
{
	int *subnet_link;
	struct ip_host *buffer;
	int subnet_size;
};
struct insert_thread_parameters
{
	int *subnet_link;
	struct ip_host *buffer;
	int *free_buffer;
	int *current_index;
	int test_no;
	int size;
};
struct pair
{
	char key[max_size];
	char value[max_size];
};

int convert_value_to_int(char *ch)
{
	int i=0;
	while(ch[i]!='\0')
		i++;
	int val=0,place=1,len=i;
	for(i=len-1;i>=0;i--)
	{
		val+=place*(ch[i]-48);
		place*=10;
	}
	return val;
}
void readConfig(struct pair *p)
{
	FILE *fp;
	printf("\ninside");
	fp=fopen("config.js","r");
	if(fp==NULL)
	{
		printf("\n file can not be opened");
		return;
	}
	char ch;
	int i,j=0,flag=-1,flag1=0;
	while((ch=fgetc(fp))!=EOF)
	{
		//		printf("%c",ch);
		if(ch==' ' || ch=='\n')
			continue;
		//can add comments handling logic here in extended version
		else if(flag && ch=='{')
		{
			flag=0;
		}
		else if(!flag)
		{
			if(ch==',')
				continue;
			else if(ch=='}')
			{
				flag=1;
				break;
			}
			else if(ch==':')
			{
			//	printf("\n here");
				flag1=1;
			}
			else
			{

				if(!flag1)
				{
					if(ch=='"')
					{
						i=0;
						while((ch=fgetc(fp))!='"')
						{
							p[j].key[i]=ch;
					//		printf("k=%c",p[j].key[i]);
							i++;
						}
					//	ch=fgetc(fp);
					//	printf("ch=%c",ch);
					}
				}
				else if(flag1)
				{
					if(ch=='"')
					{
						i=0;
						while((ch=fgetc(fp))!='"')
						{
							p[j].value[i]=ch;
			//				printf("v=%c",p[j].value[i]);						
							i++;
						}
					//	ch=fgetc(fp);
					//	printf("ch=%c",ch);			
						flag1=0;
						j++;
					}
				}
			}
		}
	}
	fclose(fp);
	int m,val;
	for(m=0;m<j;m++)
	{
		val=convert_value_to_int(p[m].value);
		if(strcmp(p[m].key,"SubnetMask")==0)
		{
			subnet_size=val;
		}
		else if(strcmp(p[m].key,"BufferPoolSize")==0)
		{
			BUFFER_SIZE=val;	
		}
		else if(strcmp(p[m].key,"CleanTimeout")==0)
		{
			EXPIRY_TIME=val;
		}
		else if(strcmp(p[m].key,"PrintTimeout")==0)
		{
			display_time_out=val;
		}
		else
		{
			output_type=val;
		}
	}
}
void generateRandomIP()
{
	int i;
	for(i=0; i<IP_PARTS; i++)
	{
		ipMask[i]=rand()%256;
	}
}
int map_IP_subnet_to_decimal(int *ipMask,int subnet_size)
{
	//hops will determine the number of times an ip part needs to be shifted depending upon their position in ip
	int hops = subnet_size/IP_PARTS_SIZE;
	//shift will determine the number of bits in the borrowed ip part of subnet needs to be converted into the decimal
	int shift = subnet_size%IP_PARTS_SIZE;
	int mapped_subnet_value=0;
	int i;
	for(i=0; i<hops; i++)
	{
		mapped_subnet_value+=(ipMask[i]<<(i*IP_PARTS_SIZE));
	}
	//making a number using 8 1's through character ascii value 255,left shifting it 8-subnet_size number of times and ANDing that with the ip part that is borrowed
	if(shift!=0)
	{
		unsigned char and_char=255;
		and_char=(and_char<<(IP_PARTS_SIZE-shift));
		mapped_subnet_value+=(((ipMask[hops] & (int)and_char)>>(IP_PARTS_SIZE-shift))<<(hops*IP_PARTS_SIZE));
	}
	return mapped_subnet_value;
}

int map_IP_host_to_decimal(int *ipMask,int host_size)
{
	//hops will determine the number of times an ip part needs to be shifted depending upon their position in ip
	int hops = host_size/IP_PARTS_SIZE;
	//shift will determine the number of bits in the borrowed ip part of subnet needs to be converted into the decimal
	int shift = host_size%IP_PARTS_SIZE;
	int mapped_host_value=0;
	int i;
	for(i=0; i<hops; i++)
	{
		mapped_host_value+=(ipMask[IP_PARTS-i-1]<<(i*IP_PARTS_SIZE));
	}
	//making a number using 8 1's through character ascii value 255,left shifting it 8-subnet_size number of times and ANDing that with the ip part that is borrowed
	if(shift!=0)
	{
		unsigned char and_char=255;
		and_char=(and_char>>(IP_PARTS_SIZE-shift));
		mapped_host_value+=(ipMask[IP_PARTS-hops-1] & (int)and_char)<<(hops*IP_PARTS_SIZE);
	}
	return mapped_host_value;
}

//display thread
void *display_statistics(void *vargp)
{
	//initializing parameters for display thread
	struct display_thread_parameters *param=(struct display_thread_parameters*)vargp;
	int *subnet_link=param->subnet_link;
	struct ip_host *buffer=param->buffer;
	int size=param->subnet_size;
	int i;

	int val;

	char fname[200];
	FILE *fp;
	sprintf(fname,"ip_dump_%lu.csv",(unsigned long)time(NULL));
	fp=fopen(fname,"a");
	fprintf(fp,"IP,COUNT,LAST_UPDATED_TIMESTAMP\n");

	//finding the middle val to find the type of lock to apply
	if(size%2==0)
		val=size/2;
	else
		val=size/2+1;

	while(1)
	{
		// Sleep(display_time_out*1000);
		sleep(display_time_out);
		for(i=0; i<size; i++)
		{
			//lock of ranges for a particular subnet
			if(pthread_mutex_trylock(&lock[i/(size/2)])==0)
			{
				if(subnet_link[i]!=NIL)
				{
					int temp = subnet_link[i];
					while(temp!=NIL)
					{
						fprintf(fp,"%d.%d.%d.%d,%lu\n",buffer[temp].ip[0],buffer[temp].ip[1],buffer[temp].ip[2],buffer[temp].ip[3],buffer[temp].count);
						//printf("\nIP - %d.%d.%d.%d, COUNT- %lu",buffer[temp].ip[0],buffer[temp].ip[1],buffer[temp].ip[2],buffer[temp].ip[3],buffer[temp].count);
						temp=buffer[temp].next_host_index;
					}
				}
				pthread_mutex_unlock(&lock[i/(size/2)]);
				//unlock
			}
			/*else
			  Sleep(0.5);*/
		}
		fclose(fp);
		sprintf(fname,"ip_dump_%lu.csv",(unsigned long)time(NULL));
		fp=fopen(fname,"a");
		fprintf(fp,"IP,COUNT,LAST_UPDATED_TIMESTAMP\n");
		if(k!=0)
		{
		if((unsigned long)time(NULL)-start_time >=15 && (unsigned long)time(NULL)-start_time <25)
		{
			printf("\n15 sec completed\n");
			pthread_exit(NULL);
		}
		else if((unsigned long)time(NULL)-start_time >= 25)
		{
			printf("\n25 sec completed\n");
			pthread_exit(NULL);
		}
		}
	}
}
void *clean_up(void *vargp)
{
	//printf("\n000000000000000cleanup strated  %lu",(unsigned long)time(NULL));
	//initializin gthe parameters
	struct cleanup_thread_parameters *param = (struct cleanup_thread_parameters*)vargp;
	int lower=param->lower;
	int upper=param->upper;
	int *subnet=param->subnet;
	struct ip_host *buffer=param->buffer;
	int *free_buffer=param->free_buffer;
	int *current_index=param->current_index;
	int size=param->subnet_size;

	int i;
	while(1)
	{
		//printf("\ninside cleanup lower=%d, upper=%d",lower,upper);
		//sleep(1);
		//looping in a range of subnets dependin gupon the thraed
		for(i=lower; i<upper; i++)
		{
			//printf("\ninside clean up loop for %d",i);
			//sleep(1);
			//lock with range getting from upper parameter of this thread
			if(pthread_mutex_trylock(&lock[(upper-1)/(size/2)])==0)
			{
				//printf("\nlock acquired in cleanup with index = %d",subnet[i]);
				//sleep(1);
				int next_index=subnet[i];
				//getting current time in ms
				unsigned long current_time;
				int temp;
				while(next_index!=NIL)
				{
					current_time=(unsigned long)time(NULL);
					//printf("\n--%lu--",current_time);
					//sleep(1);
					//exzpiry cehck
					if(current_time - buffer[next_index].last_updated_timestamp >= EXPIRY_TIME)
					{
						//cleanup at subnet cinnected node
						if(subnet[i]==next_index)
						{
							if(pthread_mutex_trylock(&lock[NO_OF_LOCKS])==0)
							{
								//                      printf("\n*** going to delete %d.%d.%d.%d with %lu at %lu\n",buffer[next_index].ip[0],buffer[next_index].ip[1],buffer[next_index].ip[2],buffer[next_index].ip[3],buffer[next_index].last_updated_timestamp,current_time);
								subnet[i] = buffer[next_index].next_host_index;
								buffer[next_index].next_host_index=NIL;
								buffer[next_index].next_host_index=NIL;
								//lock for free1 buffer
								//                        printf("\nlock acq");
								(*current_index)=(*current_index)+1;
								free_buffer[*current_index]=next_index;
								//                      printf("\nfree current index = %d      current index=%d \n",free_buffer[*current_index],*current_index);
								if((*current_index)==BUFFER_SIZE)
								{
									printf("\nBUFFER EMPTY after %lu sec",(unsigned long)time(NULL)-start_time);
									pthread_mutex_unlock(&lock[NO_OF_LOCKS]);
									break;
								}
								pthread_mutex_unlock(&lock[NO_OF_LOCKS]);
							}
							/*else
							  Sleep(0.5);	*/
							//unlock
							//                printf("\n*** deleted %d.%d.%d.%d at %lu\n",buffer[next_index].ip[0],buffer[next_index].ip[1],buffer[next_index].ip[2],buffer[next_index].ip[3],current_time);
							next_index=subnet[i];
							temp=next_index;
						}
						//clean up in middle or last node in a subnet
						else
						{
							if(pthread_mutex_trylock(&lock[NO_OF_LOCKS])==0)
							{
								//              printf("\n*** going to delete %d.%d.%d.%d with %lu at %lu\n",buffer[next_index].ip[0],buffer[next_index].ip[1],buffer[next_index].ip[2],buffer[next_index].ip[3],buffer[next_index].last_updated_timestamp,current_time);
								buffer[temp].next_host_index=buffer[next_index].next_host_index;
								buffer[next_index].next_host_index=NIL;
								//lock for free1 buffer
								//                printf("\nlock acq");
								*current_index=(*current_index)+1;
								free_buffer[*current_index]=next_index;
								//              printf("\nfree current index = %d   current index = %d\n",free_buffer[*current_index],*current_index);
								if((*current_index)==BUFFER_SIZE)
								{
									printf("\nBUFFER EMPTY after %lu sec",(unsigned long)time(NULL)-start_time);
									pthread_mutex_unlock(&lock[NO_OF_LOCKS]);
									break;
								}
								pthread_mutex_unlock(&lock[NO_OF_LOCKS]);

							}
							/*else
							  Sleep(0.5);	*/
							//unlock
							//        printf("\n*** deleted %d.%d.%d.%d at %lu\n",buffer[next_index].ip[0],buffer[next_index].ip[1],buffer[next_index].ip[2],buffer[next_index].ip[3],current_time);
							next_index=buffer[temp].next_host_index;

						}
					}
					//updating values to advance through nodes in a subnet
					else
					{
						temp=next_index;
						next_index=buffer[temp].next_host_index;
					}
				}
				pthread_mutex_unlock(&lock[(upper-1)/(size/2)]);
				//unlock
			}
			/*else
			  Sleep(0.5);*/
		}
		if(k!=0)
		{
		if((unsigned long)time(NULL)-start_time > ((iteration/iteration_size)*(sleep_per_iteration)+EXPIRY_TIME))
		{
			printf("\n clean up ends after %lu sec",(unsigned long)time(NULL)-start_time);
			pthread_exit(NULL);
		}
		}
		//sleep(2);
		//Sleep(2000);
	}
}
void inputGenerator(int n)
{
	static unsigned int i=1;
	ipMask[0]=10;
	ipMask[1]=i%n;
	ipMask[2]=12;
	ipMask[3]=13;
	i++;
}
void inputGeneratorS(int n)
{
	static unsigned int i=1;
	ipMask[0]=10;
	ipMask[1]=10;
	ipMask[2]=12;
	ipMask[3]=i%n;
	i++;
}
void inputGen()
{
	static unsigned int i=1;
	ipMask[0]=i%40;
	ipMask[1]=i%250;
	ipMask[2]=12;
	ipMask[3]=13;
	i++;

}
void *insert(void *vargp)
{
	struct insert_thread_parameters *param=(struct insert_thread_parameters*)vargp;
	int *subnet_link=param->subnet_link;
	struct ip_host *buffer=param->buffer;
	int *free_buffer=param->free_buffer;
	int k=param->test_no;
	int *current_index=param->current_index;
	int size=param->size;
	int host_size=IP_SIZE-subnet_size;
	//main insertion thread
	int j=1,i;
	int next_free_buffer_index=0;
	while(j<=iteration)
	{
		if(k==1)
			inputGeneratorS(unique_ip);
		else if(k==2 || k==3 || k==4)
			inputGenerator(unique_ip);
		else if(k==5)
			inputGen();
		else
			generateRandomIP();
		//printf("\n IP CAME - %d.%d.%d.%d",ipMask[0],ipMask[1],ipMask[2],ipMask[3]);
		//mapping subnet bits in generated ip to decimal
		//printf("\n %d.%d.%d.%d--%d",ipMask[0],ipMask[1],ipMask[2],ipMask[3],subnet_size);
		int subnet_mapped_value = map_IP_subnet_to_decimal(ipMask,subnet_size);
		//printf("\nsubnet - %d\n",subnet_mapped_value);
		//mapping host bits in generated ip to decimal
		int mapped_host_value = map_IP_host_to_decimal(ipMask,host_size);
		//getting the middle elemeents to divide the locks and threads accordingly
		//getting current time in ms
		unsigned long current_time;
		current_time=(unsigned long )time(NULL);
		//lock of range depends upon the subnet divided range on the basis of subnet mapped value
		if(pthread_mutex_trylock(&lock[subnet_mapped_value/(size/2)])==0)
		{
			int next_index = subnet_link[subnet_mapped_value];
			//checking for empty subnet insertion and then inserting on the basis of first time empty buffer or next time from free buffer
			if(next_index==NIL)
			{
				if(next_free_buffer_index<BUFFER_SIZE)
				{
					subnet_link[subnet_mapped_value] = next_free_buffer_index;
					//getting the node ready
					buffer[next_free_buffer_index].mapped_host_value = mapped_host_value;
					for(i=0; i<IP_PARTS; i++)
					{
						buffer[next_free_buffer_index].ip[i]=ipMask[i];
					}
					buffer[next_free_buffer_index].count = 1;
					buffer[next_free_buffer_index].next_host_index = NIL;
					buffer[next_free_buffer_index].last_updated_timestamp = current_time;
					//          printf("\n*********************\nNode inserted for ip  %d.%d.%d.%d at buffer index - %d at time - %lu",buffer[next_free_buffer_index].ip[0],buffer[next_free_buffer_index].ip[1],buffer[next_free_buffer_index].ip[2],buffer[next_free_buffer_index].ip[3],next_free_buffer_index,current_time);
					next_free_buffer_index++;
					if(next_free_buffer_index==BUFFER_SIZE)
						printf("\nBuffer Exhausted after %lu sec",(unsigned long)time(NULL)-start_time);
				}
				else
				{
					//lock
					if(pthread_mutex_trylock(&lock[NO_OF_LOCKS])==0)
					{
						//            printf("\nlock acquired for again insert");
						if(*current_index!=0 && *current_index<=BUFFER_SIZE)
						{
							subnet_link[subnet_mapped_value] = free_buffer[*current_index];
							//              printf("\n---------------------from free buffer at %d for current index = %d for ip - %d.%d.%d.%d",free_buffer[*current_index],*current_index,ipMask[0],ipMask[1],ipMask[2],ipMask[3]);
							//getting the node ready
							buffer[free_buffer[*current_index]].mapped_host_value = mapped_host_value;
							for(i=0; i<IP_PARTS; i++)
							{
								buffer[free_buffer[*current_index]].ip[i]=ipMask[i];
							}
							buffer[free_buffer[*current_index]].count = 1;
							buffer[free_buffer[*current_index]].next_host_index = NIL;
							buffer[free_buffer[*current_index]].last_updated_timestamp = current_time;
							*current_index=*current_index-1;
							if(*current_index==0)
								printf("\nBuffer exhausted after %lu sec",(unsigned long)time(NULL)-start_time);
							//printf("\nafter current index = %d -- %d\n",current_index,free_buffer[current_index]);
						}
						else
						{
							//packet unhandled
							packet_unhandled++;
							//            printf("\n---------------------lost ip - %d.%d.%d.%d",ipMask[0],ipMask[1],ipMask[2],ipMask[3]);
						}
						pthread_mutex_unlock(&lock[NO_OF_LOCKS]);
					}
					else
					{
						//          printf("\n---------********packet drop %lu",(unsigned long)time(NULL));
						packet_drop++;
					}
					//unlock
				}
			}
			//insertion for empty subnet compkete
			//insertion for filled subnet starts if there is space in buffer
			else
			{
				int temp,flag=1;
				//reaching to a point where we need to add tehr node or finding whether the same host is present in the subnet
				while(next_index!=NIL)
				{
					temp = next_index;
					if(buffer[next_index].mapped_host_value==mapped_host_value)
					{
						flag=0;
						break;
					}
					next_index = buffer[next_index].next_host_index;
				}
				//reached at insertionm and upadation point
				//checing whethet to update otr insert
				if(flag)
				{
					//inserting first timein bvuffere
					if(next_free_buffer_index<BUFFER_SIZE)
					{
						//getting the node ready
						buffer[next_free_buffer_index].mapped_host_value = mapped_host_value;
						for(i=0; i<IP_PARTS; i++)
						{
							buffer[next_free_buffer_index].ip[i]=ipMask[i];
						}
						buffer[next_free_buffer_index].count = 1;
						buffer[next_free_buffer_index].next_host_index = NIL;
						buffer[temp].next_host_index = next_free_buffer_index;
						buffer[next_free_buffer_index].last_updated_timestamp = current_time;
						//    printf("\n*********************\nNode inserted for ip  %d.%d.%d.%d at buffer index - %d at time - %lu",buffer[next_free_buffer_index].ip[0],buffer[next_free_buffer_index].ip[1],buffer[next_free_buffer_index].ip[2],buffer[next_free_buffer_index].ip[3],next_free_buffer_index,current_time);
						next_free_buffer_index++;
						if(next_free_buffer_index==BUFFER_SIZE)
							printf("\nBuffer Exhausted after %lu sec",(unsigned long)time(NULL)-start_time);
					}
					//inserting after cleanup
					else
					{
						//lock
						if(pthread_mutex_trylock(&lock[NO_OF_LOCKS])==0)
						{
							//      printf("\nlock acquired for again insert");
							if(*current_index!=0 && *current_index<=BUFFER_SIZE)
							{
								//getting the node ready
								buffer[free_buffer[*current_index]].mapped_host_value = mapped_host_value;
								//        printf("\n---------------------from free buffer at %d for current index = %d for ip - %d.%d.%d.%d",free_buffer[*current_index],*current_index,ipMask[0],ipMask[1],ipMask[2],ipMask[3]);
								for(i=0; i<IP_PARTS; i++)
								{
									buffer[free_buffer[*current_index]].ip[i]=ipMask[i];
								}
								buffer[free_buffer[*current_index]].count = 1;
								buffer[free_buffer[*current_index]].next_host_index = NIL;
								buffer[temp].next_host_index = free_buffer[*current_index];
								buffer[free_buffer[*current_index]].last_updated_timestamp = current_time;
								*current_index=*current_index-1;
								if(*current_index==0)
									printf("\nBuffer exhausted after %lu sec",(unsigned long)time(NULL)-start_time);
								//printf("\nafter current index = %d -- %d\n",current_index,free_buffer[current_index]);
							}
							else
							{
								//packet unhandled
								packet_unhandled++;
								//      printf("\n---------------------lost ip - %d.%d.%d.%d",ipMask[0],ipMask[1],ipMask[2],ipMask[3]);
							}
							pthread_mutex_unlock(&lock[NO_OF_LOCKS]);
							//unlock
						}
						else
						{
							//printf("\n---------********packet drop %lu",(unsigned long)time(NULL));
							packet_drop++;
						}
					}
				}
				//upadtion happens here
				else
				{
					buffer[temp].count+=1;
					buffer[temp].last_updated_timestamp = current_time;
					// printf("\n---------------------updated ip - %d.%d.%d.%d",ipMask[0],ipMask[1],ipMask[2],ipMask[3]);
				}
			}
			pthread_mutex_unlock(&lock[subnet_mapped_value/(size/2)]);
			//unlock
		}
		else
			packet_drop++;


		/*if(flag1 && next_free_buffer_index==BUFFER_SIZE-1)
		  {
		  flag1=0;
		//printf("\n inside");
		struct cleanup_thread_parameters *param1,*param2;
		struct cleanup_thread_parameters param3,param4;
		param1=&param3;
		param2=&param4;
		param1->lower=0;
		param1->upper=size/(NO_OF_THREADS)+2;
		param1->subnet=subnet_link;
		param1->buffer=buffer;
		param1->free_buffer=free_buffer;
		param1->current_index=&current_index;
		param1->subnet_size=size;
		pthread_create(&clean_thread[0],NULL,clean_up,(void*)param1);
		//	printf("\nNot created");
		param2->lower=size/(NO_OF_THREADS)+2;
		param2->upper=size;
		param2->subnet=subnet_link;
		param2->buffer=buffer;
		param2->free_buffer=free_buffer;
		param2->current_index=&current_index;
		param2->subnet_size=size;
		pthread_create(&clean_thread[1],NULL,clean_up,(void*)param2);
		}	*/
		if(k!=0)
		{
			if(j%iteration_size==0)
			{
				// Sleep(sleep_per_iteration*1000);
				sleep(sleep_per_iteration);
				//	printf("\n%d came",j);
			}
			if(k==5)
				iteration_size=5000;
			j++;
		}
	}
	pthread_exit(NULL);
	//main insertion thread code ends
}
int main()
{
	//test case input
	printf("\nenter the test case no.  - ");
	scanf("%d",&k);
	//taking the input of subnet bits and buffer size


	if(k==1 || k==2)
	{
		subnet_size=16;
		BUFFER_SIZE=10;
		EXPIRY_TIME=2;
		display_time_out=3;
		unique_ip=20;
		iteration=100;
		sleep_per_iteration=1;
		iteration_size=10;
	}
	else if(k==3)
	{
		subnet_size=16;
		BUFFER_SIZE=10;
		EXPIRY_TIME=15;
		display_time_out=3;
		unique_ip=5;
		iteration=100;
		sleep_per_iteration=1;
		iteration_size=10;
	}
	else if(k==4)
	{
		subnet_size=16;
		BUFFER_SIZE=10;
		EXPIRY_TIME=2;
		display_time_out=3;
		unique_ip=10;
		iteration=10;
		iteration_size=10;
	}
	else if(k==5)
	{	
		subnet_size=16;
		BUFFER_SIZE=10000;
		EXPIRY_TIME=10;
		display_time_out=3;
		unique_ip=10000;
		iteration=15000;
		sleep_per_iteration=5;
		iteration_size=10000;
	}
	/*else if(k==6)
	{
		sleep_per_iteration=20;
		display_time_out=60;
		EXPIRY_TIME=30;
		iteration=15000;
	}*/
	else
	{
		struct pair p[max_size];
		readConfig(p);
		//printf("\n%d=%d=%d=%d",subnet_size,BUFFER_SIZE,EXPIRY_TIME,display_time_out);
		/*printf("\nEnter number of subnet mask - ");
		scanf("%d",&subnet_size);
		if(subnet_size>=32 || subnet_size<=0)
		{
			printf("\nInvalid Subnet Size");
			return 0;
		}
		printf("\nEnter buffer size - ");
		scanf("%d",&BUFFER_SIZE);
		printf("\nEnter clean time out - ");
		scanf("%d",&EXPIRY_TIME);
		printf("\nEnter display time out - ");
		scanf("%d",&display_time_out);*/
	}
	// getting the size of our primary data structure
	int size=(1<<subnet_size);
	printf("\n%d--",size);
	//inputs taken
	//initializing the buffer
	struct ip_host *buffer=(struct ip_host*)malloc(sizeof(struct ip_host)*BUFFER_SIZE);
	int i;
	if(buffer==NULL)
	{
		printf("\n cannot allocate buffer");
		return 0;
	}
	for(i=0; i<BUFFER_SIZE; i++)
	{
		buffer[i].count=0;
		buffer[i].next_host_index = NIL;
	}
	//getting the free_buffer READY
	int *free_buffer=(int*)malloc(sizeof(int*)*(BUFFER_SIZE+1));
	int current_index=0;
	/*initializing the locks*/
	if(pthread_mutex_init(&lock[0],NULL) && pthread_mutex_init(&lock[1],NULL) && pthread_mutex_init(&lock[NO_OF_LOCKS],NULL))
	{
		printf("\n-------------\nmutex lock can not be initialized\n----------------\n");
		return 0;
	}
	// locks initializig done
	//allocating memory to array of subnet links
	int *subnet_link = (int*)malloc(sizeof(int*)*size);
	if(subnet_link==NULL)
	{
		printf("\nMemory allocation issue");
		return 0;
	}
	//initializing the primary DS with -1 i.e NIL
	memset(subnet_link,NIL,sizeof(int*)*size);
	//getting recent time in ms
	unsigned long current_time;
	current_time=(unsigned long)time(NULL);
	start_time=current_time;
	//initializing the threads i.e cleanup and display
	pthread_t clean_thread[NO_OF_THREADS],display_thread,insertion_thread;

	struct insert_thread_parameters *ins_param,ins_param1;
	ins_param=&ins_param1;
	ins_param->buffer=buffer;
	ins_param->subnet_link=subnet_link;
	ins_param->current_index=&current_index;
	ins_param->free_buffer=free_buffer;
	ins_param->test_no=k;
	ins_param->size=size;
	pthread_create(&insertion_thread,NULL,insert,(void*)ins_param);

	int flag1=1,flag2=1;
	while(1)
	{
		current_time=(unsigned long)time(NULL);
		if(!flag1 && !flag2)
			break;
		if(flag1 && current_time-start_time >= display_time_out)
		{
			flag1=0;
			/*getting the parameters ready for display thread*/
			struct display_thread_parameters *display_param1;
			struct display_thread_parameters temp;
			display_param1=&temp;
			display_param1->subnet_link=subnet_link;
			display_param1->buffer=buffer;
			display_param1->subnet_size=size;
			pthread_create(&display_thread,NULL,display_statistics,(void*)display_param1);
			//display thread creation done
		}
		if(flag2 && current_time-start_time >= EXPIRY_TIME)
		{
			flag2=0;
			/*getting the paramethers for clean up threads ready*/
			struct cleanup_thread_parameters *param[NO_OF_THREADS],param1[NO_OF_THREADS];
			int lower=0;
			int upper=size/(NO_OF_THREADS);
			int remains=size%NO_OF_THREADS;
			for(i=0;i<NO_OF_THREADS;i++)
			{
				param[i]=&param1[i];
				param[i]->lower=lower;
				param[i]->upper=upper;
				param[i]->subnet=subnet_link;
				param[i]->buffer=buffer;
				param[i]->free_buffer=free_buffer;
				param[i]->current_index=&current_index;
				param[i]->subnet_size=size;
				pthread_create(&clean_thread[i],NULL,clean_up,(void*)param[i]);
				lower=upper;
				if(i!=NO_OF_THREADS-2)
					upper+=upper;
				else
					upper+=(upper+remains);
			}
			//cleanup thread creation done
		}
	}

	(void) pthread_join(insertion_thread,NULL);
	(void) pthread_join(display_thread, NULL);
	(void) pthread_join(clean_thread[0], NULL);
	(void) pthread_join(clean_thread[1], NULL);
	printf("\nPACKET DROP = %d  PACKET UNHANDLED = %d\n",packet_drop,packet_unhandled);


	//freeing the memory
	free(subnet_link);
	free(free_buffer);
	free(buffer);

	return 0;
}
