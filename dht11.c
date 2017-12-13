#include <wiringPi.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>  
#include <curl/curl.h>
#include <stdlib.h>

//定义DATA针脚为WIRINGPI的0号
#define DHT11PIN                   0
//定义了检测的时间间隔为 10秒，每10秒检测一次温度湿度。
#define REPORT_PERIOD_MS      10000
/*
MAXCOUNT也是需要根据不同的代码来确定的。
定义了最大的检测计数。当检测计数超过这个值，说明通讯出现了错误
因为我的loopCheckUntilStateChange每次检测都有1us的延时。
通讯过程中最长状态的持续时间只有80us.因此阈值设置为100
*/
#define MAX_COUNT               100

//#define _DEBUG
/*
当检测到的状态不是state的时候就返回。
如果参数state是HIGH，那么需要等到检测到LOW的时候返回。
返回值代表了状态改变所用的时间计数。
返回值如果是-1 表明和设备通讯出现了错误，需要重新开始通讯。
*/
int loopCheckUntilStateChange(int state)
{
	//初始时间计数为0
	int count=0;
	//读取针脚的状态
	while(digitalRead(DHT11PIN)==state)
	{
		//如果状态没变那么计数加1
		count++;
		//如果计数超过了最大的计数，表明通讯错误，需要重新开始
		if(count>MAX_COUNT)
			return -1;
		//延时1微秒。
		delayMicroseconds(1);
	} 
	return count;
}


bool readDHT11(float* ptemp,float* phumi)
{

	int count=0,ibit=0;
	bool bret=false;
	//保存电平改变所用的时间计数
	int bitsz[40]={0};
	//保存读取到的4个温湿度数据和1个校验码
	int data[5]={0};

	//首先设置为输出模式
	pinMode(DHT11PIN,OUTPUT);
	//写入低电平
        digitalWrite(DHT11PIN,LOW);
	//低电平至少持续18ms。保证能够DHT检测到这个初始信号
	delay(20);
	//随后主机将电平拉高，通知DHT信号结束
	digitalWrite(DHT11PIN,HIGH);
	//将针脚转为输入模式，等待DHT的响应。
	pinMode(DHT11PIN,INPUT);
	//经过20-40us，我们需要等待DHT将电平从高电平拉低
	if(loopCheckUntilStateChange(HIGH)<0)
	{
#ifdef _DEBUG
		printf("DHT11 no response first  h2l\n");
#endif		
		return bret;
	}
	//DHT的响应信号低电平持续80us，等待DHT再次由低拉高
	if(loopCheckUntilStateChange(LOW)<0)
	{
#ifdef _DEBUG
		printf("DHT11 no response l2h\n");
#endif
		return bret;
	}
	//高电平将持续80us,等待DHT将高电平拉低。开始传输数据
	if(loopCheckUntilStateChange(HIGH)<0)
	{
#ifdef _DEBUG
		printf("DHT11 module error\n");
#endif
		return bret;
	}
	
	//数据传输开始。每次传输一个bit。一共40bit。5字节
	for(ibit=0;ibit<40;ibit++)
	{
		//每个bit传输由50us的低电平开始。变为高电平就是开始传输0或者1了。
		if(loopCheckUntilStateChange(LOW)<0)
		{
#ifdef _DEBUG
			printf("DHT11 error while sending (50us low )\n");
#endif
			return bret;
		}
		//这次我们要检测高电平到低电平经过了多少个计数的count
		//以此来决定这一bit是0还是1
		count=loopCheckUntilStateChange(HIGH);
		//如果小于0，出错了
		if(count<0)
		{
#ifdef _DEBUG
			printf("DHT11 error while sending (0 or 1)\n");
#endif
			return bret;
		}
		//将高电平持续的count保存在bitsz里面。
		bitsz[ibit]=count;
	}

/*
定义了区别 0 和1 的count 阈值。这个阈值要根据实际情况和你写的函数调整。
比如 loopCheckUntilStateChange里面如果没有delayMicroseconds(1);完全跑CPU，那么count值会比较大。
如果有delayMicroseconds(1)那么这个值会小一些。
*/
#define THRESHOLD 20
	for(ibit=0;ibit<40;ibit++)
	{
		/*
		将data左移1位，因为ibit的传输是从高位到低位
		每次填充后就要移动一次。
		ibit/8 确定是第几个data
		*/
		data[ibit/8]<<=1;
		/*
		根据传输时高电平占用时间的count来确定这个bit是0还是1
		THRESHOLD阈值根据不同的情况来确定，1的时间长，count大。0的时间短。count小。确定
		一个中间的值即可	
		*/
		if(bitsz[ibit]>THRESHOLD)
		{
			//如果超过了阈值。说明这一位是1。否则什么都不做。默认就是0了
			data[ibit/8]|=1;
		}
		
	}
	//检查校验码。查看整个数据是否传输准确了
	if(data[4]!=(data[3]+data[2]+data[1]+data[0])&0xff)
	{
#ifdef _DEBUG
		printf("check sum error %08x %08x \n",data[4],(data[3]+data[2]+data[1]+data[0])&0xff);
#endif
		return bret;
	}
	
	/*
	由于dht11设备默认小数位为0，没有小数位.所以这里小数位就不要了。只有整数位的温度和湿度。
	否则就是(%d.%d data[0].data[1])湿度 (%d.%d data[2].data[3])温度
	*/
	*ptemp=data[2]*1.0;
	*phumi=data[0]*1.0;
	

	return true;
}

size_t showResponse(void *buffer, size_t size, size_t nmemb, void *userp);
#define POST_URL "http://api.heclouds.com/devices/5837917/datapoints?type=3" //MODIFY YOUR YOURDEVICEID
void postData(float temp,float humi)
{
	CURL *curl;
	char jsonData[512];
	struct curl_slist* header=NULL;
	printf("humi=%.2f temp=%.2f\n",humi,temp);
	curl = curl_easy_init();  
	curl_easy_setopt(curl, CURLOPT_URL, POST_URL);  
	header=curl_slist_append(NULL,"api-key:dXwwjXNhaVN=wtwuwDS0RS=u9vg=");//MODIFY YOUR APPKEY
	curl_slist_append(header,"content-type:application/json");
	sprintf(jsonData,"{\"temp\":\"%.2f\",\"humi\":\"%.2f\"}",temp,humi);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonData); 
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, showResponse); 
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header);  
	curl_easy_setopt(curl, CURLOPT_COOKIEFILE, "/tmp/iotpost.cookie");  
	curl_easy_perform(curl); 
	curl_slist_free_all(header);
	curl_easy_cleanup(curl);
};
size_t showResponse(void *buffer, size_t size, size_t nmemb, void *userp)
{
	char* response=NULL;
	if(!buffer||!size||!nmemb)
	{
		printf("no response\n");
		return;
	}
	response=(char*)malloc(size*nmemb+1);
	memcpy(response,buffer,size*nmemb);
	response[size*nmemb]=0;
	printf("response: %s\n\n",response);
	free((void*)response);
};

void uploadTempHumi()
{
	float temp=0,humi=0;
	int trytimes=0;
#define MAX_TRY_TIMES 10
	for (trytimes=0;trytimes<MAX_TRY_TIMES;trytimes++)
	{
		if(readDHT11(&temp,&humi))
		{
			postData(temp,humi);
			return;
		}
	}
	printf("try %d times,but failed\n",MAX_TRY_TIMES);
}




int main()
{
 bool a=1;
 wiringPiSetup();


 while(a)
{
  uploadTempHumi();
  delay(REPORT_PERIOD_MS);
}


 
}
