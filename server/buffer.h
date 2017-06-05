#ifndef BUFFER_H_SENTRY
#define BUFFER_H_SENTRY



class InoutBuffer {
    char *data;
    int datalen;
    int maxlen;
public:
    InoutBuffer();
    ~InoutBuffer();

    void AddData(const void *buf, int size);
    int GetData(void *buf, int bufsize);
    void DropData(int len);
    void EraseData(int index, int len);
    void DropAll() { datalen = 0; } 

    void AddChar(char c);
    void AddString(const char *str);
#if 0
    int ReadLine(char *buf, int bufsize);
#endif
    bool ReadLine(InoutBuffer &buf);
    int ReadCRLFLine(char *buf, int bufsize);
    bool ReadCRLFLine(InoutBuffer &buf);
    
#if 0
          /*! returns the index of the '\n' right after the marker */ 
    int FindLineMarker(const char *marker) const;
    int ReadUntilLineMarker(const char *marker, char *buf, int bufsize);
    bool ReadUntilLineMarker(const char *marker, InoutBuffer &dest);

    bool ContainsExactText(const char *str) const;
#endif
    
    const char *GetBuffer() const { return data; }
    
    int Length() const { return datalen; }
    char& operator[](int i) const { return *(data + i); }

private:
    void ProvideMaxLen(int n);
};





#endif // SENTRY
