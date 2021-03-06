#include "Curl.h"
#include <utility>
#include "client.h"
#include "Utils.h"

using namespace std;
using namespace ADDON;

Curl::Curl()
= default;

Curl::~Curl()
= default;

string Curl::GetCookie(const string& name)
{
  if (cookies.find(name) == cookies.end())
  {
    return "";
  }
  return cookies[name];
}

void Curl::AddHeader(const string& name, const string& value)
{
  headers[name] = value;
}

void Curl::AddOption(const string& name, const string& value)
{
  options[name] = value;
}

void Curl::ResetHeaders()
{
  headers.clear();
}

string Curl::Delete(const string& url, int &statusCode)
{
  return Request("DELETE", url, "", statusCode);
}

string Curl::Get(const string& url, int &statusCode)
{
  return Request("GET", url, "", statusCode);
}

string Curl::Post(const string& url, const string& postData, int &statusCode)
{
  return Request("POST", url, postData, statusCode);
}

string Curl::Request(const string& action, const string& url, const string& postData,
    int &statusCode)
{
  void* file = XBMC->CURLCreate(url.c_str());
  if (!file)
  {
    statusCode = -1;
    return "";
  }

  XBMC->CURLAddOption(file, XFILE::CURL_OPTION_PROTOCOL, "customrequest",
      action.c_str());

  XBMC->CURLAddOption(file, XFILE::CURL_OPTION_HEADER, "acceptencoding",
      "gzip");
  if (!postData.empty())
  {
    string base64 = Base64Encode((const unsigned char *) postData.c_str(),
        postData.size(), false);
    XBMC->CURLAddOption(file, XFILE::CURL_OPTION_PROTOCOL, "postdata",
        base64.c_str());
  }

  for (auto const &entry : headers)
  {
    XBMC->CURLAddOption(file, XFILE::CURL_OPTION_HEADER, entry.first.c_str(),
        entry.second.c_str());
  }

  for (auto const &entry : options)
  {
    XBMC->CURLAddOption(file, XFILE::CURL_OPTION_PROTOCOL, entry.first.c_str(),
        entry.second.c_str());
  }

  if (!XBMC->CURLOpen(file, XFILE::READ_NO_CACHE))
  {
    statusCode = 403; //Fake statusCode for now
    return "";
  }
  int numValues;
  char **cookiesPtr = XBMC->GetFilePropertyValues(file,
      XFILE::FILE_PROPERTY_RESPONSE_HEADER, "set-cookie", &numValues);

  for (int i = 0; i < numValues; i++)
  {
    char *cookiePtr = cookiesPtr[i];
    if (cookiePtr && *cookiePtr)
    {
      string cookie = cookiePtr;
      std::string::size_type paramPos = cookie.find(';');
      if (paramPos != std::string::npos)
        cookie.resize(paramPos);
      vector<string> parts = Utils::SplitString(cookie, '=', 2);
      if (parts.size() != 2)
      {
        continue;
      }
      cookies[parts[0]] = parts[1];
      XBMC->Log(LOG_DEBUG, "Got cookie: %s.", parts[0].c_str());
    }
  }
  XBMC->FreeStringArray(cookiesPtr, numValues);
  
  char *tmp = XBMC->GetFilePropertyValue(file,
      XFILE::FILE_PROPERTY_RESPONSE_HEADER, "Location");
  location = tmp != nullptr ? tmp : "";
  
  XBMC->FreeString(tmp);


  // read the file
  static const unsigned int CHUNKSIZE = 16384;
  char buf[CHUNKSIZE + 1];
  ssize_t nbRead;
  string body;
  while ((nbRead = XBMC->ReadFile(file, buf, CHUNKSIZE)) > 0 && ~nbRead)
  {
    buf[nbRead] = 0x0;
    body += buf;
  }

  XBMC->CloseFile(file);
  statusCode = 200;
  return body;
}

std::string Curl::Base64Encode(unsigned char const* in, unsigned int in_len,
    bool urlEncode)
{
  std::string ret;
  int i(3);
  unsigned char c_3[3];
  unsigned char c_4[4];

  const char *to_base64 =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

  while (in_len)
  {
    i = in_len > 2 ? 3 : in_len;
    in_len -= i;
    c_3[0] = *(in++);
    c_3[1] = i > 1 ? *(in++) : 0;
    c_3[2] = i > 2 ? *(in++) : 0;

    c_4[0] = (c_3[0] & 0xfc) >> 2;
    c_4[1] = ((c_3[0] & 0x03) << 4) + ((c_3[1] & 0xf0) >> 4);
    c_4[2] = ((c_3[1] & 0x0f) << 2) + ((c_3[2] & 0xc0) >> 6);
    c_4[3] = c_3[2] & 0x3f;

    for (int j = 0; (j < i + 1); ++j)
    {
      if (urlEncode && to_base64[c_4[j]] == '+')
        ret += "%2B";
      else if (urlEncode && to_base64[c_4[j]] == '/')
        ret += "%2F";
      else
        ret += to_base64[c_4[j]];
    }
  }
  while ((i++ < 3))
    ret += urlEncode ? "%3D" : "=";
  return ret;
}
