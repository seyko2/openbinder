/*
 * Copyright (c) 2005 Palmsource, Inc.
 * 
 * This software is licensed as described in the file LICENSE, which
 * you should have received as part of this distribution. The terms
 * are also available at http://www.openbinder.org/license.html.
 * 
 * This software consists of voluntary contributions made by many
 * individuals. For the exact contribution history, see the revision
 * history and logs, available at http://www.openbinder.org
 */

#include <support/StringUtils.h>

#include <support/SharedBuffer.h>

#if _SUPPORTS_NAMESPACE
using namespace palmos::support;
#endif

            
status_t StringSplit(const SString& srcStr, const SString& splitOn, SVector<SString>* strList, bool append)
{
    return StringSplit((const char*)srcStr, srcStr.Length(), (const char*)splitOn, splitOn.Length(), strList, append);
}


status_t StringSplit(const char* srcStr, int32_t srcStrLen, const char* splitOn, int32_t splitOnLen, SVector<SString>* strList, bool append)
{
    char* srcStart = (char*)srcStr;
    char* srcPos = (char*)srcStr;

    char* sponPos = (char*)splitOn;

    char* srcEnd = (char*)(srcStr + srcStrLen);
    char* sponEnd = (char*)(sponPos + (splitOnLen));

    if(!append)
        strList->MakeEmpty();

    do
    {
        char* roundSrcPos = srcPos;

        //Check to see if splitOn will fit into the rest of the source string
        if(!((srcEnd - srcPos) > splitOnLen))
        {
            //If there is not enough room left in the source string to hold the splitOn
            //string then just add the rest of the string into the string list
            strList->AddItem(SString(srcStart, (int32_t)(srcEnd - srcStart)));
            break;
        }

        //If the first character of the string matches then check the whole string
        do
        {
            if(*srcPos != *sponPos)
                break;
            
            //If we are at the end of the splitOn string break out before we increment
            //the pointers.
            if(!(sponPos < sponEnd))
                break;
            srcPos++;
            sponPos++;
        }while(true);
        
        //Check to see if the match was long enough
        if(sponPos == sponEnd)
        {
            //It looks like we had a complete match

            //A match should never finish in the middle of a double byte string so we can
            //just bump by one
            strList->AddItem(SString(srcStart, (int32_t)(roundSrcPos - srcStart)));

            //Set srcStart to the first character after the match
            srcStart = srcPos;
        }
        else
        {
            //Set srcPos back to the start position of this round so we can bump it
            //to the start of the next character
            srcPos = roundSrcPos;

            //If this is the start of a double byte character bump by one
            if(!(*srcPos & 0x80))
                srcPos++;
            else
            {
                srcPos += 2;

                if(srcPos > srcEnd)
                {
                    //We have encountered the start of a double byte character at the
                    //end of the string.
                    return B_ERROR;
                }
            }
        }

        //Reset the sponPos variable to the start of the splitOn string
        sponPos = (char*)splitOn;
    }while(srcPos < srcEnd);
    
    return B_OK;
}

