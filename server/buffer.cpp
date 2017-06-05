#include "buffer.h"


#include <string.h>


InoutBuffer::InoutBuffer()
{
    data = new char[maxlen = 512];
    datalen = 0;
}

InoutBuffer::~InoutBuffer()
{
    delete[] data;
}

void InoutBuffer::AddData(const void *buf, int size)
{
    ProvideMaxLen(datalen + size);
    memcpy(data + datalen, buf, size);
    datalen += size;
}

void InoutBuffer::AddChar(char c)
{
    ProvideMaxLen(datalen + 1);
    data[datalen] = c;
    datalen++;
}

int InoutBuffer::GetData(void *buf, int size)
{
    if(datalen == 0)
        return 0; // a bit of optimization ;-)
    if(size >= datalen) { // all data to be read/removed
        memcpy(buf, data, datalen);
        int ret = datalen;
        datalen = 0;
        return ret;
    } else { // only a part of the data to be removed
        memcpy(buf, data, size);
        DropData(size);
        return size;
    }
}

void InoutBuffer::DropData(int size)
{
    EraseData(0, size);
}

void InoutBuffer::EraseData(int index, int size)
{
    if(index+size>=datalen) {
        datalen = index;
    } else {
        memmove(data+index, data+index+size, datalen - (index+size));
        datalen -= size;
    }
}

void InoutBuffer::AddString(const char *str)
{
    AddData(str, strlen(str));
}

#if 0
int InoutBuffer::ReadLine(char *buf, int bufsize)
{
    int crindex = -1;
    for(int i=0; i< datalen; i++)
        if(data[i] == '\n') { crindex = i; break; }
    if(crindex == -1) return 0;
    if(crindex >= bufsize) { // no room for the whole string
        memcpy(buf, data, bufsize-1);
        buf[bufsize-1] = 0;
        DropData(crindex+1);
    } else {
        GetData(buf, crindex+1);
        //assert(buf[crindex] == '\n');
        buf[crindex] = 0;
        if(buf[crindex - 1] == '\r')
            buf[crindex-1] = 0;
    }
    return crindex + 1;
}
#endif

bool InoutBuffer::ReadLine(InoutBuffer &dest)
{
    int crindex = -1;
    for(int i=0; i< datalen; i++)
        if(data[i] == '\n') { crindex = i; break; }
    if(crindex == -1) return false;
    dest.ProvideMaxLen(crindex+1);
    GetData(dest.data, crindex+1);
    //assert(dest.data[crindex] == '\n');
		dest.datalen = crindex;
    dest.data[crindex] = 0;
    if(dest.data[crindex - 1] == '\r')
        dest.data[crindex-1] = 0;
    return true;
}

int InoutBuffer::ReadCRLFLine(char *buf, int bufsize)
{
    int crindex = -1;
    for(int i=0; i< datalen-1; i++)
        if(data[i]=='\r' && data[i]=='\n') { crindex = i; break; }
    if(crindex == -1) return 0;
    int dlen = crindex < bufsize-1 ? crindex : bufsize-1;
    memcpy(buf, data, dlen);
    buf[dlen] = 0;
    DropData(crindex+2);
    return crindex+1;
}

bool InoutBuffer::ReadCRLFLine(InoutBuffer &dest)
{
    int crindex = -1;
    for(int i=0; i< datalen-1; i++)
        if(data[i]=='\r' && data[i]=='\n') { crindex = i; break; }
    if(crindex == -1) return false;
    dest.ProvideMaxLen(crindex+1);
    memcpy(dest.data, data, crindex);
    dest.data[crindex] = 0;
    DropData(crindex+2);
    return true;
}

#if 0
int InoutBuffer::FindLineMarker(const char *marker) const
{
    for(int i = 0; i< datalen; i++) {
        if(data[i] == '\r') i++;
        if(data[i] != '\n') continue;
        bool found = true;
        int j = 1;
        for(const char *p = marker;*p;p++,j++) {
            if(/* data[i+j]=='\0' ||*/ data[i+j] != *p)
                { found = false; break; }
        }
        if(!found) continue;
        if(data[i+j] == '\r') j++;
        if(data[i+j] != '\n') continue;
        return i+j;
    }
    return -1;
}

int InoutBuffer::ReadUntilLineMarker(const char *marker, char *buf, int bufsize)
{
    int ind = FindLineMarker(marker);
    if(ind == -1) return -1;
    if(bufsize >= ind+1) {
        return GetData(buf, ind+1);
    } else {
        return GetData(buf, bufsize);
    }
}

bool InoutBuffer::ReadUntilLineMarker(const char *marker, InoutBuffer &dest)
{
    int ind = FindLineMarker(marker);
    if(ind == -1) return false;
    dest.ProvideMaxLen(ind+2);
    GetData(dest.data, ind+1);
    dest.data[ind+1] = 0;
    dest.datalen = ind+1;
    return true;
}

bool InoutBuffer::ContainsExactText(const char *str) const
{
    int i;
    for(i=0; (i<datalen) && (str[i]!=0); i++) {
        if(str[i]!=data[i]) return false;
    }
    return (i==datalen) && (str[i]==0);
}
#endif

void InoutBuffer::ProvideMaxLen(int n)
{
    if(n <= maxlen) return;
    int newlen = maxlen;
    while(newlen < n) newlen*=2;
    char *newbuf = new char[newlen];
    memcpy(newbuf, data, datalen);
    delete [] data;
    data = newbuf;
    maxlen = newlen;
}

