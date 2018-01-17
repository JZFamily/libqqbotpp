#include "NetworkWrapper.h"
#include <functional>
#include <iostream>
#include <algorithm>
#include "json.hpp"
using namespace std;
using namespace nlohmann;

void Delay(int second);
string UTF8ToGBK(string UTF8String);
string GBKToUTF8(string GBKString);
void OpenQRCode();

const string USERAGENT="Mozilla/5.0 (Windows NT 6.3; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/61.0.3163.79 Safari/537.36";

string qrsig;

int GetQRCode()
{
    HTTPConnection t;
    t.setUserAgent(USERAGENT);
    t.setURL("https://ssl.ptlogin2.qq.com/ptqrshow?appid=501004106&e=0&l=M&s=5&d=72&v=4&t=0.1");
    t.setDataOutputFile("qrcode.png");
    t.setCookieOutputFile("cookie1.txt");
    t.perform();
    vector<Cookie> vec=t.getCookies();
    for(auto& c:vec)
    {
        if(c.name=="qrsig")
        {
            qrsig=c.value;
        }
    }

    OpenQRCode();
    printf("二维码已打开. 请使用手机扫描二维码进行登录.\n");

    return 0;
}

void ReplaceTag(std::string& src,const std::string& tag,const std::string& replaceto)
{
    size_t x;
    while((x=src.find(tag))!=string::npos)
    {
        src.replace(x,tag.size(),replaceto);
    }
}

void _DoStrParse(std::string& out, int current_cnt, const std::string& current_str)
{
    char buf[16];
    sprintf(buf,"{%d}",current_cnt);
    string tag(buf);

    ReplaceTag(out,tag,current_str);
}

template<typename... Args>
void _DoStrParse(std::string& out,int current_cnt,const std::string& current_str,Args&&... args)
{
    _DoStrParse(out,current_cnt,current_str);
    current_cnt++;

    _DoStrParse(out,current_cnt,args...);
}

template<typename... Args>
std::string StrParse(const std::string& src,Args&&... args)
{
    int cnt=1;
    std::string s(src);
    _DoStrParse(s,cnt,args...);
    return s;
}

//用于生成ptqrtoken的哈希函数
string hash33(const std::string& s)
{
    int e = 0;
    int sz=s.size();
    for (int i = 0; i<sz; ++i)
    {
        e += (e << 5) + (int)s[i];
    }
    int result=2147483647 & e;
    char buff[16];
    sprintf(buff,"%d",result);
    return string(buff);
}

string step2url;

int WaitForQRScan()
{
    while(true)
    {
        HTTPConnection t;
        t.setVerbos(true);
        t.setUserAgent(USERAGENT);
        string url_raw="https://ssl.ptlogin2.qq.com/ptqrlogin?"
                    "ptqrtoken={1}&webqq_type=10&remember_uin=1&login2qq=1&aid=501004106&"
                    "u1=https%3A%2F%2Fw.qq.com%2Fproxy.html%3Flogin2qq%3D1%26webqq_type%3D10&"
                    "ptredirect=0&ptlang=2052&daid=164&from_ui=1&pttype=1&dumy=&fp=loginerroralert&0-0-157510&"
                    "mibao_css=m_webqq&t=undefined&g=1&js_type=0&js_ver=10184&login_sig=&pt_randsalt=3";
        string url_ans=StrParse(url_raw,hash33(qrsig));

        t.setURL(url_ans);
        t.setReferer("https://ui.ptlogin2.qq.com/cgi-bin/login?daid=164&target=self&style=16&mibao_css=m_webqq&appid=501004106&enable_qlogin=0&no_verifyimg=1&s_url=http%3A%2F%2Fw.qq.com%2Fproxy.html&f_url=loginerroralert&strong_login=1&login_state=10&t=0.1");
        t.setCookieInputFile("cookie1.txt");
        t.setCookieOutputFile("cookie2.txt");

        char buff[1024];
        t.setDataOutputBuffer(buff,1024);
        t.perform();

        printf("Buff: %s\n",UTF8ToGBK(buff).c_str());
        char* p=strstr(buff,"http");
        if(p)
        {
            char* q=strstr(p,"'");
            char xbuf[1024];
            memset(xbuf,0,1024);
            memcpy(xbuf,p,q-p);

            printf("URL Got!\n");
            break;
        }
        Delay(5);
    }
    return 0;
}

string ptwebqq;

int GetPtWebQQ()
{
    HTTPConnection t;
    t.setVerbos(true);
    t.setUserAgent(USERAGENT);
    t.setURL(step2url);
    t.setReferer("http://s.web2.qq.com/proxy.html?v=20130916001&callback=1&id=1");
    t.setCookieInputFile("cookie2.txt");
    t.setCookieOutputFile("cookie3.txt");
    t.perform();

    vector<Cookie> vec=t.getCookies();
    for(auto& c:vec)
    {
        if(c.name=="ptwebqq")
        {
            ptwebqq=c.value;
            break;
        }
    }

    return 0;
}

string vfwebqq;

int GetVfWebQQ()
{
    HTTPConnection t;
    t.setVerbos(true);
    t.setUserAgent(USERAGENT);

    string url_raw="https://s.web2.qq.com/api/getvfwebqq?ptwebqq={1}&clientid=53999199&psessionid=&t=0.1";
    string url=StrParse(url_raw,ptwebqq);

    t.setURL(url);
    t.setReferer("http://s.web2.qq.com/proxy.html?v=20130916001&callback=1&id=1");
    t.setCookieInputFile("cookie3.txt");
    t.setCookieOutputFile("cookie4.txt");
    char buff[4096];
    t.setDataOutputBuffer(buff,4096);
    t.perform();

    printf("VfWebQQBuff: %s\n",buff);

    /// 修复exception
    json j=json::parse(buff);
    vfwebqq=j["vfwebqq"];

    return 0;
}

int GetUinPsessionid()
{
    HTTPConnection t;
    t.setVerbos(true);
    t.setUserAgent(USERAGENT);
    t.setMethod(HTTPConnection::Method::Post);

    t.setURL("http://d1.web2.qq.com/channel/login2");
    t.setReferer("http://d1.web2.qq.com/proxy.html?v=20151105001&callback=1&id=2");
    t.setCookieInputFile("cookie4.txt");
    t.setCookieOutputFile("cookie5.txt");
    char buff[4096];
    t.setDataOutputBuffer(buff,4096);
    t.perform();

    printf("%s\n",buff);

    return 0;
}

class Client
{

};

int main()
{
    GetQRCode();
    WaitForQRScan();
    GetPtWebQQ();
    GetVfWebQQ();
    GetUinPsessionid();

    return 0;
}
