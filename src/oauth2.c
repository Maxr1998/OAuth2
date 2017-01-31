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

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "oauth2.h"
#include "curl_request.h"

oauth2_config* oauth2_init(char* client, char* secret)
{
    int input_strlen;
    oauth2_config* retVal = malloc(sizeof(oauth2_config));

    if(retVal == NULL)
        return NULL;

    //Copy in the client id etc
    input_strlen = strlen(client)+1;
    retVal->client_id = malloc(input_strlen * sizeof(char));
    strcpy(retVal->client_id, client);

    assert(retVal->client_id[input_strlen-1] == '\0');

    input_strlen = strlen(secret)+1;
    retVal->client_secret = malloc(input_strlen);
    strcpy(retVal->client_secret, secret);

    assert(retVal->client_secret[input_strlen-1] == '\0');

    retVal->redirect_uri = NULL;

    //Clear the error
    retVal->last_error.error = OAUTH2_ERROR_NO_ERROR;
    retVal->last_error.error_description = NULL;
    retVal->last_error.error_uri = NULL;
    retVal->last_error.state = NULL;
    retVal->auth_code = NULL;
}

void oauth2_set_redirect_uri(oauth2_config* conf, char* redirect_uri)
{
    int input_strlen;

    assert(conf != NULL);

    input_strlen = strlen(redirect_uri)+1;
    conf->redirect_uri = malloc(sizeof(char) * input_strlen);
    strcpy(conf->redirect_uri, redirect_uri);
}

void oauth2_set_auth_code(oauth2_config* conf, char* auth_code)
{
    int input_strlen;

    assert(conf != NULL);

    input_strlen = strlen(auth_code)+1;
    conf->auth_code = malloc(sizeof(char) * input_strlen);
    strcpy(conf->auth_code, auth_code);
}

char* oauth2_request_auth_code(oauth2_config* conf, char* auth_server, char* scope, char* state)
{
    int core_len;
    int scope_len;
    int state_len;
    char* core_fmt;
    char* scope_fmt;
    char* state_fmt;
    char* final_str;

    scope_len = 1;
    state_len = 1;

    assert(conf != NULL);

    //We just need to build the request string, since we can't actually handle the callback ourselves
    //URL Format: <server>?response_type=code&client_id=<client_id>&redirect_uri=<redir_uri>&scope=<scope>&state=<state>
    //Get the final length
    core_fmt = "%s?response_type=code&client_id=%s&redirect_uri=%s";
    scope_fmt = "&scope=%s";
    state_fmt = "&state=%s";

    //Get the string lengths
    core_len = snprintf(NULL, 0, (const char*)core_fmt, auth_server, conf->client_id, conf->redirect_uri) + 1;
    if(scope != NULL)
        scope_len = snprintf(NULL, 0, (const char*)scope_fmt, scope) + 1;
    if(state != NULL)
        state_len = snprintf(NULL, 0, (const char*)state_fmt, state) + 1;

    //Actually build the string
    final_str = malloc(((core_len-1)
                        +(scope_len-1)
                        +(state_len-1)+1)*sizeof(char));

    sprintf(final_str, (const char*)core_fmt, auth_server, conf->client_id, conf->redirect_uri);
    if(scope != NULL)
        sprintf((char*)(final_str+(core_len-1)),
                (const char*)scope_fmt,
                scope);

    if(state != NULL)
        sprintf((char*)(final_str+(core_len-1)+(scope_len-1)),
                (const char*)state_fmt,
                state);

    return final_str;
}

oauth2_tokens oauth2_access_tokens(oauth2_config* conf, char* auth_server, char* auth_code, bool is_refresh_token)
{
    //Build up the request
    oauth2_tokens res;
    char* grant_type = is_refresh_token ? "refresh_token" : "authorization_code";
    char* code_arg = is_refresh_token ? "refresh_token" : "code";
    char* query_fmt;
    int query_len;
    char* uri;
    char* output;

    char *acc_find_rule = "\"access_token\" : \"";
    char *ref_find_rule = "\"refresh_token\" : \"";
    char* acc_code = NULL;
    char* ref_code = NULL;
    char* pos_s;
    char* pos_e;
    int pos_len;

    assert(conf != NULL);
    assert(auth_server != NULL);
    assert(auth_code != NULL);

    query_fmt = "grant_type=%s&client_id=%s&client_secret=%s&%s=%s&redirect_uri=%s";

    query_len = snprintf(NULL, 0, query_fmt, grant_type, conf->client_id, conf->client_secret, code_arg, auth_code, conf->redirect_uri);
    uri = malloc(sizeof(char)*query_len);
    sprintf(uri, query_fmt, grant_type, conf->client_id, conf->client_secret, code_arg, auth_code, conf->redirect_uri);
    output = curl_make_request(auth_server, uri);
    free(uri);

    //Strip out the access token
    pos_s = NULL;
    pos_e = NULL;
    pos_len = 0;
    pos_s = strstr(output, acc_find_rule);
    if(pos_s != NULL)
    {
        pos_s += strlen(acc_find_rule); //Skip past rule
        //Find the end of the token
        pos_e = pos_s;
        while(*pos_e != '\"' && *pos_e != '\0') ++pos_e;

        //Now extract it
        pos_len = pos_e-pos_s;
        acc_code = malloc(pos_len);

        strncpy(acc_code, pos_s, pos_len);
        *(acc_code + pos_len) = '\0'; //Manually end the string at the right position
    }

    //Strip out the refresh token
    pos_s = NULL;
    pos_e = NULL;
    pos_len = 0;
    pos_s = strstr(output, ref_find_rule);
    if(pos_s != NULL)
    {
        pos_s += strlen(ref_find_rule); //Skip past rule
        //Find the end of the token
        pos_e = pos_s;
        while(*pos_e != '"' && *pos_e != '\0') ++pos_e;

        //Now extract it
        pos_len = pos_e-pos_s;
        ref_code = malloc(pos_len);

        strncpy(ref_code, pos_s, pos_len);
        *(ref_code + pos_len) = '\0'; //Manually end the string at the right position
    }
    free(output);

    res.access_token = acc_code;
    res.refresh_token = ref_code;
    return res;
}

char* oauth2_access_resource_owner(oauth2_config* conf, char* auth_server, char* username, char* password)
{
    char* uri;
    char* query_fmt;
    char* output;
    int query_len;

    assert(conf != NULL);
    assert(auth_server != NULL);
    assert(username != NULL);
    assert(password != NULL);

    query_fmt = "grant_type=password&client_id=%s&username=%s&password=%s";

    //Get the length of the query
    query_len = snprintf(NULL, 0, query_fmt, conf->client_id, username, password);

    //Allocate space for it and request
    uri = malloc(query_len+1);

    sprintf(uri, query_fmt, conf->client_id, username, password);

    //Now make the request!
    output = curl_make_request(auth_server, uri);

    //Cleanup
    free(uri);

    return output;
}

char* oauth2_access_refresh_token(oauth2_config* conf, char* refresh_token)
{
    assert(0);
    return NULL;
}

char* oauth2_request(oauth2_config* conf, char* uri, char* params)
{
    //For now, we'll just include the access code with the request vars
    //This is discouraged, but I don't know if most providers actually
    //support the header-field method (Facebook is still at draft 0...)

    char* retVal;
    char* uri2;
    int uri_len;

    //Sanity checks
    assert(conf != NULL);
    assert(conf->client_id != NULL);
    assert(conf->auth_code != NULL);
    assert(uri != NULL);

    //Are we POSTing?
    if(params != NULL)
    {
        //Attach the token to the params
        uri_len = snprintf(NULL, 0, "%s&access_token=%s", params, conf->auth_code);
        uri2 = malloc(sizeof(char)*uri_len);
        sprintf(uri2, "%s&access_token=%s", params, conf->auth_code);

        retVal = curl_make_request(uri, uri2);
        free(uri2);
        return retVal;
    }
    else
    {
        return NULL; //I'm not doing this now.
    }
}

void oauth2_cleanup(oauth2_config* conf)
{
    if(conf == NULL)
        return;

    //Start freeing stuff up
    if(conf->client_id != NULL)
        free(conf->client_id);

    if(conf->client_secret != NULL)
        free(conf->client_secret);

    free(conf);
}

void oauth2_tokens_cleanup(oauth2_tokens tokens)
{
    free(tokens.access_token);
    free(tokens.refresh_token);
}