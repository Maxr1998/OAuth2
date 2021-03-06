/*
Copyright (c) 2010 Jamie Garside

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#include "curl_request.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <curl/curl.h>

struct Storage {
  char *data;
  size_t size;
};

size_t curl_callback(void *response, size_t size, size_t nmemb, void *userdata)
{
    size_t realsize = size * nmemb;
    struct Storage *mem = (struct Storage *)userdata;

    mem->data = realloc(mem->data, mem->size + realsize + 1);
    if(mem->data == NULL) {
      /* out of memory! */
      printf("not enough memory (realloc returned NULL)\n");
      return 0;
    }

    memcpy(&(mem->data[mem->size]), response, realsize);
    mem->size += realsize;
    mem->data[mem->size] = 0;

    return realsize;
}

char* curl_make_request(char* url, char* params)
{
    return curl_request(NULL, url, NULL, METHOD_POST, params);
}

char* curl_request(CURL* h, char* url, struct curl_slist *headers, int method, char* params)
{
    struct Storage storage;
    CURL* handle;
    int data_len;

    assert(url != 0);
    assert(*url != 0);

    storage.data = malloc(1);
    storage.size = 0;

    if (h != NULL)
    {
      handle = h;
      curl_easy_reset(handle);
    }
    else
    {
      handle = curl_easy_init();
    }

    curl_easy_setopt(handle, CURLOPT_URL, url);
    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, curl_callback);
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, (void *)&storage);

    if (headers != NULL)
    {
      curl_easy_setopt(handle, CURLOPT_HTTPHEADER, headers);
    }

    if ((method == METHOD_POST || method == METHOD_PUT) && params != NULL)
    {
      if (method == METHOD_POST)
      {
          curl_easy_setopt(handle, CURLOPT_POST, 1);
      }
      else if (method == METHOD_PUT)
      {
          curl_easy_setopt(handle, CURLOPT_CUSTOMREQUEST, "PUT");
      }
      curl_easy_setopt(handle, CURLOPT_COPYPOSTFIELDS, params); //Copy them just in case the user does something stupid
    }

    if(curl_easy_perform(handle) != 0)
    {
        //Error!
        curl_easy_cleanup(handle);
        free(storage.data);
        return NULL;
    }

    //Cleanup
    if (h == NULL)
    {
      curl_easy_cleanup(handle);
    }
    return storage.data;
}