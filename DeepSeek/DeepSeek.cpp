/*
Copyright (c) 2025 ZhiYuanNJ <zhiyuannj@foxmail.com>
﻿
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:
﻿
The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.
﻿
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#include <iostream>
#include "Curl7.64.1/x86/include/curl/curl.h"
#include <Windows.h>
#include <sstream>
#include <regex>
#include "cJSON.h"
using namespace std;
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <io.h>
#include <fcntl.h>
#pragma comment(lib, "libcurl.lib")
//"deepseek-ai/DeepSeek-R1-Distill-Llama-8B"
//"deepseek-ai/DeepSeek-R1-Distill-Qwen-1.5B";
static const char *model  = "deepseek-ai/DeepSeek-R1-Distill-Qwen-1.5B";
static const char *APIKEY = "Authorization: Bearer sk-xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";

wchar_t *MultiByteToWideString(const char *mbString)
{
    size_t len = mbstowcs(NULL, mbString, 0);
    if (len == (size_t)-1) {
        return NULL;
    }

    wchar_t *wString = (wchar_t *)malloc((len + 1) * sizeof(wchar_t));
    if (wString != NULL) {
        mbstowcs(wString, mbString, len + 1);
    }
    return wString;
}

char *WideToMultiByteString(const wchar_t *wString)
{
    size_t len = wcstombs(NULL, wString, 0);
    if (len == (size_t)-1) {
        return NULL;
    }

    char *mbString = (char *)malloc(len + 1);
    if (mbString != NULL) {
        wcstombs(mbString, wString, len + 1);
    }
    return mbString;
}

// 回调函数，用于处理从服务器返回的响应数据
size_t write_callback(void *ptr, size_t size, size_t nmemb, void *userdata)
{
    // 解析
    cJSON *root = cJSON_Parse(((char *)ptr+5));
    if (strncmp((char *)ptr, "data: [DONE]", strlen("data: [DONE]")) == 0) {
        wprintf(L"\n--------------------------\n");
        return size * nmemb;
    }
    if (root == NULL) {
         /*const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL) {
             printf("ptr = %s\n", (char*)ptr);
            fprintf(stderr, "Error before: %s\n", error_ptr);
        }*/
        return size * nmemb;
    }

    // 获取choices数组
    cJSON *choices_array = cJSON_GetObjectItemCaseSensitive(root, "choices");
    if (cJSON_IsArray(choices_array)) {
        cJSON *choice_item = NULL;
        cJSON_ArrayForEach(choice_item, choices_array)
        {
            // 获取delta对象
            cJSON *message_obj = cJSON_GetObjectItemCaseSensitive(choice_item, "delta");
            if (cJSON_IsObject(message_obj)) {
                // 从message对象中获取reasoning_content
                cJSON *reasoning_content = cJSON_GetObjectItemCaseSensitive(message_obj, "reasoning_content");
                if (cJSON_IsString(reasoning_content) && reasoning_content->valuestring != NULL) {
                    wchar_t *reasoning_content_wide = MultiByteToWideString(reasoning_content->valuestring);
                    wprintf(L"%ls", reasoning_content_wide); // 输出content的内容
                    free(reasoning_content_wide);
                }

                // 从message对象中获取content
                cJSON *content = cJSON_GetObjectItemCaseSensitive(message_obj, "content");
                if (cJSON_IsString(content) && content->valuestring != NULL) {
                    wchar_t *content_wide = MultiByteToWideString(content->valuestring);
                    wprintf(L"%ls", content_wide); // 输出content的内容
                    free(content_wide);
                }
            }
        }
    } else {
        wprintf(L"Reasoning error\n");
    }
    // 释放 cJSON 对象
    cJSON_Delete(root);
    return size * nmemb;
}

int main(void)
{
    // 设置控制台代码页为65001（即UTF-8）
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    // 设置标准输入输出流为UTF-8模式
    _setmode(_fileno(stdout), _O_U16TEXT);
    _setmode(_fileno(stdin), _O_U16TEXT);
    // 设置全局区域信息为UTF-8
    setlocale(LC_ALL, "zh_CN.UTF-8");
    CURL *curl;
    CURLcode res;
    wchar_t input[256];
    // API 的 URL
    const char *url = "https://api.siliconflow.cn/v1/chat/completions";

    // 请求头
    struct curl_slist *headers = NULL;
    headers                    = curl_slist_append(headers, "Content-Type: application/json");
    headers                    = curl_slist_append(headers, APIKEY);
    // 初始化 libcurl
    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();
    if (!curl) {
        wprintf(L"curl init error");
        return -1;
    }
    while (1) {
        wprintf(L"Input Your Question: ");
        // 获取用户输入
        if (fgetws(input, sizeof(input) / sizeof(wchar_t), stdin) == NULL) {
            break; // 遇到输入错误则退出
        }
        // 去除换行符
        size_t len = wcslen(input);
        if (len > 0 && input[len - 1] == L'\n') {
            input[len - 1] = L'\0';
        }

        // 检查是否是退出命令
        if (wcscmp(input, L"exit") == 0) {
            wprintf(L"程序结束。\n");
            break;
        }
        wprintf(L"\nDeepSeek:");
        // 使用 cJSON 创建 JSON 对象
        cJSON *root         = cJSON_CreateObject();
        cJSON *messages     = cJSON_CreateArray();
        cJSON *message_item = cJSON_CreateObject();

        cJSON_AddStringToObject(message_item, "role", "user");
        char *input_mb = WideToMultiByteString(input);
        cJSON_AddStringToObject(message_item, "content", input_mb);
        free(input_mb);
        cJSON_AddItemToArray(messages, message_item);

        cJSON_AddStringToObject(root, "model", model);
        cJSON_AddItemToObject(root, "messages", messages);
        cJSON_AddBoolToObject(root, "stream", true);
        // 将 cJSON 对象转换为字符串
        char *json_data = cJSON_Print(root);
        if (curl) {
            // 设置 URL
            curl_easy_setopt(curl, CURLOPT_URL, url);

            // 设置请求头
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

            // 设置 POST 请求和数据
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_data);

            // 设置回调函数以处理响应
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);

            // 执行请求
            res = curl_easy_perform(curl);

            // 检查请求是否成功
            if (res != CURLE_OK) {
                fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            }

            // 清理
            cJSON_Delete(root); // 释放 cJSON 对象
            free(json_data);    // 释放打印出来的 JSON 字符串
        }
    }
    curl_easy_cleanup(curl);
    curl_slist_free_all(headers);
    curl_global_cleanup();
    return 0;
}