#include <wiringPi.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <curl/curl.h> 
#include <stdlib.h>

#define INFRAREDPIN 6

size_t showResponse(void *buffer, size_t size, size_t nmemb, void *userp);
#define POST_URL "http://api.heclouds.com/devices/5839542/datapoints?type=3" //MODIFY YOUR YOURDEVICEID
void postData(float human)
{
        CURL *curl;
        char jsonData[512];
        struct curl_slist* header=NULL;
    printf("hello\n");
        printf(" switch=%f\n",human);
        curl = curl_easy_init();  
        curl_easy_setopt(curl, CURLOPT_URL, POST_URL);  
        header=curl_slist_append(NULL,"api-key:pcAOjXtd=vOfL3eUBXiPNNVCj8s=");//MODIFY YOUR APPKEY
        curl_slist_append(header,"content-type:application/json");
        sprintf(jsonData,"{\"human\":\"%f\"}",human);
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


int main()
{
 float human = 0;
 int AnyBodyHere=0;
 wiringPiSetup();
//设置为输入模式
 pinMode(INFRAREDPIN,INPUT);
 //很重要。控制默认情况下此PIN处于下拉低电平状态
 pullUpDnControl(INFRAREDPIN,PUD_DOWN);
 while(true)
 {
  //每一秒检测依次状态
  delay(1000);
  //读取当前pin的输入状态
  AnyBodyHere=digitalRead(INFRAREDPIN);
  if(AnyBodyHere)
  {
     human=50;
     postData(human);
     printf("There is somebody here\n");
  }
  else
  {
     human = 0;
     postData(human);
     printf("There is no one here\n");
  }
  
 }
  
}











