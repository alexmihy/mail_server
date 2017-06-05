#include "smtpsrvs.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <ctype.h>

#include "base64/decoder.h"
#include "md5/md5.h"
#include "options.h"
#include "daemon.h"
#include "header.h"

#include <iostream>
#include <fstream>

AbstractProtocolServerSession::AbstractProtocolServerSession() 
{
    closing_flag = false;
}

AbstractProtocolServerSession::~AbstractProtocolServerSession() 
{
    // nothing to do
}

void AbstractProtocolServerSession::EatReceivedData(const void *buf, int len)
{
    inbuf.AddData(buf, len);
    HandleNewData();
}

int AbstractProtocolServerSession::
GetDataToTransmit(void *buf, int buflen) const
{
    int len = buflen < outbuf.Length() ? buflen : outbuf.Length();
    memcpy(buf, outbuf.GetBuffer(), len);
    return len; 
}

bool AbstractProtocolServerSession::ShouldWeCloseSession() const
{
    return closing_flag && (outbuf.Length() == 0);
}

void AbstractProtocolServerSession::Transmitted(int len)
{
    outbuf.DropData(len);
}





SMTPProtocolServerSession::SMTPProtocolServerSession(
    const char *a_domain,
    const UserList *an_user_list,
    MailQueue *a_mail_queue,
    int a_protocols
) : user_list(an_user_list), mail_queue(a_mail_queue) 
{
        
    domain = strdup(a_domain);
    remote_domain = 0;
    protocols = a_protocols;    
    state = st_beforehello;
    
    authenticated = 0;
    username = 0;

    max_recipients_count = server_options.max_recipients;
    recipients_address = new char* [max_recipients_count];
    memset(
        recipients_address, 
        0, 
        max_recipients_count * sizeof(*recipients_address)
    );
    sender_address = 0;
    
    outbuf.AddString("220 ");
    outbuf.AddString(domain);
    outbuf.AddString(" ");
    switch(protocols) {
        case lmtp: 
            outbuf.AddString("LMTP"); break;
        case smtp: 
            outbuf.AddString("SMTP"); break;
        case esmtp: 
        case esmtp|smtp: 
            outbuf.AddString("ESMTP"); break;
        case lmtp|smtp: 
        case lmtp|esmtp: 
        case lmtp|esmtp|smtp: 
            outbuf.AddString("ESMTP/LMTP"); break;
        default:
            outbuf.AddString("(no protocols available)"); break;
    }
    outbuf.AddString(" Service ready\r\n");
}

SMTPProtocolServerSession::~SMTPProtocolServerSession()
{
    if (domain)
        free((void*)domain);
    if (remote_domain) 
        free((void*)remote_domain);
    
    if (username)
        free((void*)username);

    if (sender_address)
        free((void*)sender_address);

    if (recipients_address) {
        for (int i = 0; i < max_recipients_count; i++)
            if (recipients_address[i])
                free((void*)recipients_address[i]);
        delete [] recipients_address;
    }
}

void SMTPProtocolServerSession::HandleNewData()
{
    InoutBuffer curline;
    while(inbuf.ReadLine(curline)) {
        switch(state) {
            case st_closed:
                continue;
            case st_waiting_authusername:
                FetchUsername(curline.GetBuffer());
                break;
            case st_waiting_authpassword:
                FetchPassword(curline.GetBuffer());
                break;
            case st_data:
                ProcessData(curline.GetBuffer());
                break;
            default:
                ProcessCommand(curline.GetBuffer());
        }
    }
}

void SMTPProtocolServerSession::RemoteEOT()
{
    GracefullyClose();
}

void SMTPProtocolServerSession::SetRemoteDomain(const char *s)
{
    if(remote_domain) free((void*)remote_domain);
    remote_domain = strdup(s);
}

void SMTPProtocolServerSession::ProcessData(const char *line)
{
    // XXXXXXXXXXXXXXXXX
    // should we enforce <CRLF>?
    if(line[0]=='.' && line[1]==0) {
        // end of the message
        if((protocols&(smtp|esmtp)) && (protocols&lmtp)) {
            // dont know how to respond, giving up
            outbuf.AddString("521 5.5.0 Don't know the protocol version "
                " Please use HELO/EHLO/LHLO\r\n");
            GracefullyClose();
            return;
        }
        if(protocols&lmtp) { // LMTP
            int *resp = new int[recipients_count];
            bool rc = MessageDataEndL(resp, recipients_count);
            for(int i=0; i<recipients_count; i++) {
                if(rc) {
                    DataEndResponse(resp[i]);
                } else {
                    DataEndResponse(554 /*failed*/);
                }
            }
            delete [] resp;
        } else {            // SMTP/ESMTP
            int rc = MessageDataEnd();
            DataEndResponse(rc);
        }
        MessageDiscard();
        state = st_beforemail; 
    } else {
            if(line[0]=='.') 
                    line++; // rfc821, 4.5.2(2)
            if(still_accepting_data && !MessageAddLine(line)) {
                    still_accepting_data = false; 
            }
    }
}

void SMTPProtocolServerSession::ProcessCommand(const char *line)
{
    // Let's assume all SMTP/ESMTP/LMTP commands must be
    // exaclty of 4 symbols
    // As far as I know that's true as of now...
    char cmd[5]; 
    unsigned int i;
    for(i=0; i<sizeof(cmd)-1; i++) 
        cmd[i]=toupper(line[i]);  
    cmd[sizeof(cmd)-1]=0;
    if(line[sizeof(cmd)-1]!=' ' && line[sizeof(cmd)-1]!=0) {
        UnrecognizedCommand();
        return;
    }
    const char *parameters = line[4]=='\0' ? "" : line+5;

                    // 3 versions of HELLO
    if(strncmp(cmd, "HELO", sizeof(cmd)-1)==0) {
        ProcessHello(parameters, smtp);
    } else
    if(strncmp(cmd, "EHLO", sizeof(cmd)-1)==0) {
        ProcessHello(parameters, esmtp);
    } else
    if(strncmp(cmd, "LHLO", sizeof(cmd)-1)==0) {
        ProcessHello(parameters, lmtp);
    } else

            // minimal set of commands as defined in rfc821
            //
    if(strncmp(cmd, "MAIL", sizeof(cmd)-1)==0) {
        ProcessMailCommand(parameters);
    } else
    if(strncmp(cmd, "RCPT", sizeof(cmd)-1)==0) {
        ProcessRcptCommand(parameters);
    } else
    if(strncmp(cmd, "DATA", sizeof(cmd)-1)==0) {
        ProcessDataCommand(parameters);
    } else
    if(strncmp(cmd, "RSET", sizeof(cmd)-1)==0) {
        ProcessRsetCommand(parameters);
    } else
    if(strncmp(cmd, "NOOP", sizeof(cmd)-1)==0) {
        ProcessNoopCommand(parameters);
    } else
    if(strncmp(cmd, "QUIT", sizeof(cmd)-1)==0) {
        ProcessQuitCommand(parameters);
    } else
            //
            // minimal set ends here
            
            // auth command
            //
    if(strncmp(cmd, "AUTH", sizeof(cmd)-1)==0) {
        ProcessAuthCommand(parameters);
    } else 
        
    {
        // some commands recognized but not implemented
        //
        const char *unimplemented[] = 
            {"SEND", "SAML", "SOML", 
            "VRFY", "EXPN", "HELP",
            "TURN", "ETRN",
            0
            };
        for(const char **p=unimplemented; *p; p++) 
            if(strncmp(cmd, *p, sizeof(cmd)-1)==0) {
                UnimplementedCommand();
                return;
            }
        UnrecognizedCommand();
        return;
    }       
}



void SMTPProtocolServerSession::ProcessHello(const char *param, int prot)
{  
    // first, check if the right HELO/EHLO/LHLO is used
    if(!(protocols&prot)) {
        // wrong!
        outbuf.AddString("501 5.5.5 wrong protocol version, try "); 
        if(protocols&lmtp)  outbuf.AddString("LHLO ");
        if(protocols&smtp)  outbuf.AddString("HELO ");
        if(protocols&esmtp) outbuf.AddString("EHLO ");
        outbuf.AddString("\r\n");
        return;
    }

    // XXXXXXXXXXXXXX
    // maybe we should check the domain.... somehow...
    SetRemoteDomain(param);

    // now it depends on the protocol version
    if(prot==smtp) { 
        // short response for smtp
        outbuf.AddString("250 ");
        outbuf.AddString(domain);
        outbuf.AddString(" Pleased to meet you\r\n"); 
    } else {
        // long response for lmtp and esmtp
        outbuf.AddString("250-");
        outbuf.AddString(domain);
        outbuf.AddString(" Pleased to meet you\r\n"); 
        outbuf.AddString("250-ENHANCEDSTATUSCODES\r\n"); 
        outbuf.AddString("250 PIPELINING\r\n"); 
    }

    // change state only if we are in the beginning state
    if(state==st_beforehello) {
        state = st_beforemail; 
        // update protocols
        if(prot==lmtp)
                protocols = lmtp;   // now we know we are lmtp!
        else 
                protocols &= ~lmtp; // now we know we are NOT lmtp!
    }
}

void SMTPProtocolServerSession::ProcessMailCommand(const char *param)
{
    if(strncmp("FROM:", param, 5)!=0) {
        outbuf.AddString("501 5.5.2 Syntax error, FROM: expected\r\n"); 
        return;
    }
    const char *addr = param+5;
    int rc;
    switch(state) {
        case st_beforehello: 
            outbuf.AddString("503 5.5.0 say hello first\r\n"); 
            break;
        case st_recipients:
            outbuf.AddString("503 5.5.0 duplicate MAIL command\r\n"); 
            break;
        case st_beforemail:
            rc = MessageStart(addr);
            switch(rc) {
                case 250: // Ok 
                    CustomizedReply("250 2.1.0 Sender Ok"); 
                    state = st_recipients;
                    recipients_count = 0;
                    break;
                case 421: // Service unavailable, closing connection
                    ServiceUnavailable(); 
                    break;
                case 451: // local error
                    CustomizedReply("451 4.1.8 Sender address rejected"); 
                    break;
                case 553: // sender_address forbidden
                    CustomizedReply("551 5.1.8 Sender address rejected"); 
                    break;
                case 503: // protocol
                default:
                    InternalError();
            }
            break;
        default:
            InternalError();
            break;
    }
}

void SMTPProtocolServerSession::ProcessRcptCommand(const char *param)
{
    if(strncmp("TO:", param, 3)!=0) {
        outbuf.AddString("501 5.5.2 Syntax error, TO: expected\r\n"); 
        return;
    }
    const char *addr = param+3;
    int rc;
    switch(state) {
        case st_beforehello: 
                outbuf.AddString("503 5.5.0 Say hello first\r\n"); 
                break;
        case st_beforemail:
                outbuf.AddString("503 5.5.0 Use MAIL command "
                    "to start a message\r\n"); 
                break;
        case st_recipients:
                rc = MessageAddRecipient(addr);
                switch(rc) {
                    case 250: // Ok 
                        CustomizedReply("250 2.1.5 Recipient Ok");
                        break;
                    case 251: // Ok 
                        CustomizedReply("251 2.1.5 Recipient Ok"); 
                        break;
                    case 421: // Service unavailable, closing connection
                        ServiceUnavailable(); 
                        break;
                    case 450: // mailbox unavailable
                        CustomizedReply("450 4.2.0 Mailbox unavailable"); 
                        break;
                    case 451: // local error
                        CustomizedReply("451 4.1.8 Recipient rejected"); 
                        break;
                    case 550: // mailbox unavailable
                        CustomizedReply("550 5.2.0 Mailbox unavailable"); 
                        break;
                    case 551: // relaying denied
                        CustomizedReply("551 5.1.2 Relaying denied"); 
                        break;
                    case 552: // too many recipients
                        CustomizedReply("552 5.5.3 Too many recipients"); 
                        break;
                    case 553: // bad address
                        CustomizedReply("553 5.1.0 Recipient rejected"); 
                        break;
                    case 503: // protocol
                    default:
                        InternalError();
            }
            break;
        default:
            InternalError();
            break;
    }

}

void SMTPProtocolServerSession::ProcessDataCommand(const char *param)
{
    if(state != st_recipients) {
        outbuf.AddString("503 5.5.0 Say MAIL first\r\n"); 
        return;
    }
    if(recipients_count==0) {
        outbuf.AddString("503 5.5.0 Need at least one recipient\r\n"); 
        return;
    }
    state = st_data;
    still_accepting_data = true;
    outbuf.AddString("354 3.3.0 Enter mail, end with <CRLF>.<CRLF>\r\n"); 
}

void SMTPProtocolServerSession::ProcessRsetCommand(const char *param)
{
    MessageDiscard();
    if(state!=st_beforehello)
        state = st_beforemail;
    outbuf.AddString("250 2.0.0 Reset state\r\n"); 
}

void SMTPProtocolServerSession::ProcessNoopCommand(const char *param)
{
    outbuf.AddString("250 2.0.0 OK, no operation done\r\n"); 
}

void SMTPProtocolServerSession::ProcessQuitCommand(const char *param)
{
    outbuf.AddString("221 2.0.0 Good bye\r\n"); 
    GracefullyClose();
    return;
}



void SMTPProtocolServerSession::ProcessAuthCommand(const char *param) 
{
    if (state == st_beforehello) {
        outbuf.AddString("503 5.5.0 say hello first\r\n");
        return;
    }
    
    if (state > st_beforemail) {
        outbuf.AddString(
            "503 AUTH command during mail transaction forbidden\r\n");
        return;
    }
    
    if (authenticated) {
        outbuf.AddString("503 duplicate AUTH command\r\n");
        return;
    }
    
    if (strncmp(param, "LOGIN", 5) != 0) {
        outbuf.AddString("504 unimplemented AUTH mechanism\r\n");
        return;
    }
    
    outbuf.AddString("334 VXNlcm5hbWU6\r\n");
    state = st_waiting_authusername;
}

void SMTPProtocolServerSession::FetchUsername(const char *line) 
{
    int linelen = strlen(line);
    if ((linelen >= 1) && (line[0] == '*') && (line[1] == '\0')) {
        outbuf.AddString("501 authentication rejected by user\r\n");
        state = st_beforemail;
        return;
    }
    
    if (username)
        free((void*)username);
    size_t username_len;
    username = Decoder::Base64Decode(line, linelen, &username_len);

    if (username == 0) {
        outbuf.AddString("535 invalid BASE64 string. Please try again\r\n");
        return;
    }
    
    outbuf.AddString("334 UGFzc3dvcmQ6\r\n");
    state = st_waiting_authpassword;
}

void SMTPProtocolServerSession::FetchPassword(const char *line) 
{
    int linelen = strlen(line);
    if ((linelen >= 1) && (line[0] == '*') && (line[1] == '\0')) {
        outbuf.AddString("501 authentication rejected by user\r\n");
        state = st_beforemail;
        return;
    }

    size_t password_len;
    char *password = Decoder::Base64Decode(line, linelen, &password_len);
    
    if (password == 0) {
        outbuf.AddString("535 invalid BASE64 string. Please try again\r\n");
        return;
    }
    
    const char *password_hash = MD5::GetMD5Hash(password);
    free((void*)password);

    int user_idx = user_list->FindUserByAddress(username);
    const char *user_password = user_list->GetPassword(user_idx);
    
    if (!user_password || strcmp(user_password, password_hash)) {
        outbuf.AddString("535 invalid username/password\r\n");
    } else {
        outbuf.AddString("235 authentication done\r\n");
        authenticated = 1;
    }

    state = st_beforemail;
    delete [] password_hash;
}




void SMTPProtocolServerSession::MessageDiscard()
{
    msg_data.DropAll();
    
    if (sender_address)
        free((void*)sender_address);
    sender_address = 0;
    
    for (int i = 0; i < max_recipients_count; i++){
        if (recipients_address[i])
            free((void*)recipients_address[i]);
        recipients_address[i] = 0;
    }
    recipients_count = 0;
}

int SMTPProtocolServerSession::MessageStart(const char *a_sender_address) 
{
    if (state != st_beforemail)
        return 503;
    
    if (FindAtSymbolInAddress(a_sender_address) < 0)
        return 553;
    
    int shift_len;
    for (shift_len = 0; 
        ((a_sender_address[shift_len] == ' ') || 
        (a_sender_address[shift_len] == '\t')) &&
        (a_sender_address[shift_len] != '\0');
         shift_len++);
    
    // handle this address format <some_name@some_domain>
    if (a_sender_address[shift_len] == '<')
        shift_len++;
    
    sender_address = strdup(a_sender_address + shift_len);
    
    int len = strlen(sender_address);
    if (sender_address[len - 1] == '>')
        sender_address[len - 1] = '\0';
    
    return 250;
}

int SMTPProtocolServerSession::MessageAddRecipient(const char *address)
{
    if (recipients_count + 1 >= max_recipients_count)
        return 552;
    
    int atpos;
    if ((atpos = FindAtSymbolInAddress(address)) < 0)
        return 553;
    
    int shift_len;
    for (shift_len = 0;
        ((address[shift_len] == ' ') || 
        (address[shift_len] == '\t')) &&
        (address[shift_len] != '\0'); 
        shift_len++);
    
    // handle this address format: <some_name@some_domain>
    if (address[shift_len] == '<')
        shift_len++;
    
    recipients_address[recipients_count] = strdup(address + shift_len);
    
    int len = strlen(recipients_address[recipients_count]);
    if (recipients_address[recipients_count][len - 1] == '>')
        recipients_address[recipients_count][len - 1] = '\0';
    
    if (!strcmp(address + atpos + 1, domain)) {
        if (user_list->FindUserByAddress(recipients_address[recipients_count]) < 0) {
            free((void*)recipients_address[recipients_count]);
            return 450;
        }
    }
    
    recipients_count++;
    return 250;
}

bool SMTPProtocolServerSession::MessageAddLine(const char *line) 
{
    if (msg_data.Length() > server_options.max_message_size)
        return 0;
    
    msg_data.AddString(line);
    msg_data.AddString("\r\n");
    
    return 1;
}

int SMTPProtocolServerSession::MessageDataEnd() 
{
    if (msg_data.Length() > server_options.max_message_size)
        return 552;
    
    //write_log("[SMTP-DAEMON] username = (%s) sender_address = (%s)\n", username, sender_address);

    int atpos = FindAtSymbolInAddress(sender_address);
    if (!strcmp(sender_address + atpos + 1, domain)) {
        if (!authenticated || strcmp(username, sender_address))
            return 550;
    } else {
        bool only_far_recipients = 1;
        for (int i = 0; i < recipients_count; i++) {
            atpos = FindAtSymbolInAddress(recipients_address[i]);
            if (!strcmp(recipients_address[i] + atpos + 1, domain)) {
                only_far_recipients = 0;
                break;
            }
        }
        
        if (only_far_recipients)
            return 550;
    }

    char *message_id = Message::GenerateMessageId(domain);

    header_parser.ReadMail(msg_data.GetBuffer(), msg_data.Length());

    time_t cur_time;
    time(&cur_time);
    const char *date = ctime(&cur_time);
    char *received_value = GenerateRecievedField(message_id);
    
    if (header_parser.GetField("Date") == 0)
        header_parser.SetField("Date", date);    
    //else 
    //    header_parser.SetField("Resent-Date", date);
    
    if (header_parser.GetField("Message-ID") == 0)
        header_parser.SetField("Message-ID", message_id);
    //else 
    //    header_parser.SetField("Resent-Message-ID", message_id);
    header_parser.AddField("Received", received_value);

    delete [] received_value;
    
    header_parser.GenerateHeader();
    msg_data.DropAll();
    msg_data.AddData(
        header_parser.GetHeader().GetBuffer(), 
        header_parser.GetHeader().Length()
    );
    msg_data.AddData(
        header_parser.GetBody().GetBuffer(),
        header_parser.GetBody().Length()
    );

    Message *message = new Message(
        message_id,
        sender_address, 
        recipients_address, recipients_count, 
        &msg_data
    );

    if (mail_queue->AddMessage(message) < 0) {
        write_log(
            "[SMTP-DAEMON] %s message could not add to queue\n", 
            message_id
        );

        delete message;
        delete [] message_id;   

        return 451;
    }
    delete [] message_id;
    
    return 250;
}




void SMTPProtocolServerSession::UnrecognizedCommand()
{
    outbuf.AddString("500 5.5.1 Command unrecognized\r\n"); 
}

void SMTPProtocolServerSession::UnimplementedCommand()
{
    outbuf.AddString("502 5.5.1 Command or feature not implemented\r\n"); 
}

void SMTPProtocolServerSession::InternalError()
{
    outbuf.AddString("421 4.3.0 ");
    outbuf.AddString(domain); 
    outbuf.AddString(" Internal server error, sorry\r\n"); 
    GracefullyClose(); 
}

void SMTPProtocolServerSession::ServiceUnavailable()
{
    CustomizedReply("421 4.3.0 Service unavailable, closing connection"); 
    GracefullyClose(); 
}

void SMTPProtocolServerSession::DataEndResponse(int code)
{
    switch(code) {
        case 250: // Ok 
            CustomizedReply("250 2.0.0 Ok"); 
            break;
        case 451: // local error
            CustomizedReply("451 4.0.0 Local error"); 
            MessageDiscard();
            break;
        case 452: // insufficient data storage
            CustomizedReply("452 4.3.4 Quota exceeded"); 
            MessageDiscard();
            break;
        case 550: // rejected
            CustomizedReply("550 5.7.1 Message rejected"); 
            MessageDiscard();
            break;
        case 552: // insufficient data storage
            CustomizedReply("552 5.3.4 Message too big"); 
            MessageDiscard();
            break;
        case 554: // delivery failed
            CustomizedReply("554 5.0.0 Failed"); 
            MessageDiscard();
            break;
        default:
            InternalError();
    }
}

void SMTPProtocolServerSession::CustomizedReply(const char *r)
{
    outbuf.AddString(r); 
    const char *s = MessageCustomComment();
    if(s) {
        outbuf.AddString(" ("); 
        outbuf.AddString(s); 
        outbuf.AddString(")"); 
    }
    outbuf.AddString("\r\n"); 
}

int SMTPProtocolServerSession::FindAtSymbolInAddress(const char *address) 
{
    int res_pos = -1, at_count = 0;
    for (int i = 0; address[i] != '\0'; i++) {
        if (address[i] == '@')
            res_pos = i, at_count++;
    }
    
    return at_count == 1 ? res_pos: -1;
}

char* SMTPProtocolServerSession::GenerateRecievedField(const char *message_id) const
{
    char *result = new char [1024];
    result[0] = '\0';

    strcat(result, "from ");
    strcat(result, remote_domain);
    strcat(result, " (helo=[");
    strcat(result, remote_domain);
    strcat(result, "])\r\n\tby ");
    strcat(result, domain);
    strcat(result, " with smtp id ");
    strcat(result, message_id);

    return result;
}
