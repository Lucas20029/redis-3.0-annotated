/* Select()-based ae.c module.
 *
 * Copyright (c) 2009-2012, Salvatore Sanfilippo <antirez at gmail dot com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Redis nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */


#include <string.h>

typedef struct aeApiState {
    fd_set rfds, wfds;
    /* We need to have a copy of the fd sets as it's not safe to reuse
     * FD sets after select(). */
    fd_set _rfds, _wfds;
} aeApiState;

static int aeApiCreate(aeEventLoop *eventLoop) {
    aeApiState *state = zmalloc(sizeof(aeApiState));

    if (!state) return -1;
    FD_ZERO(&state->rfds);
    FD_ZERO(&state->wfds);
    eventLoop->apidata = state;
    return 0;
}

static int aeApiResize(aeEventLoop *eventLoop, int setsize) {
    /* Just ensure we have enough room in the fd_set type. */
    if (setsize >= FD_SETSIZE) return -1;
    return 0;
}

static void aeApiFree(aeEventLoop *eventLoop) {
    zfree(eventLoop->apidata);
}

static int aeApiAddEvent(aeEventLoop *eventLoop, int fd, int mask) {
    aeApiState *state = eventLoop->apidata;

    if (mask & AE_READABLE) FD_SET(fd,&state->rfds);
    if (mask & AE_WRITABLE) FD_SET(fd,&state->wfds);
    return 0;
}

static void aeApiDelEvent(aeEventLoop *eventLoop, int fd, int mask) {
    aeApiState *state = eventLoop->apidata;

    if (mask & AE_READABLE) FD_CLR(fd,&state->rfds);
    if (mask & AE_WRITABLE) FD_CLR(fd,&state->wfds);
}

static int aeApiPoll(aeEventLoop *eventLoop, struct timeval *tvp) {
    aeApiState *state = eventLoop->apidata;
    int retval, j, numevents = 0;

    memcpy(&state->_rfds,&state->rfds,sizeof(fd_set));
    memcpy(&state->_wfds,&state->wfds,sizeof(fd_set));

    //CC:
    //select ������ ��ָ��ʱ���ڣ�����ָ�����ļ���������File Descriptor,��дΪFD�������ҷ���Ready����Щ�ļ���
    //�������壺
    //1. maxfd ���ļ����������Χ+1������select���Լ��������Χ�ڵ������ļ�
    //2. rfds��Read File Descriptor Set����fd_set���͡��������ΪC#�� List<�ļ�������>��Select�������Щ�ļ��Ķ��仯��ͨ��FD_ISSETϵͳ�������ж�
    //3. wfds��Write File Descriptor Set������д�仯�����ǹ����Ƿ��������Щ�ļ���д�������ˣ���������������һ���ļ���д��select�ͻ᷵��һ������0��ֵ����ʾ���ļ���д��
    //4. errorfds��
    //5. tvp����ʱʱ��

    //tvp��select�ĳ�ʱʱ�䣬�������������Ҫ��������ʹselect��������״̬:
    //1: tvp = null����������ʱ��ṹ�����ǽ�select��������״̬��һֱ�ȵ������ļ�������������ĳ���ļ������������仯Ϊֹ��
    //2: tvp = 0��0���룬�ͱ��һ������ķ����������������ļ��������Ƿ��б仯��select���������̷��ؼ���ִ�У��ļ��ޱ仯����0���б仯����һ����ֵ��
    //3: tvp ����0������ǵȴ��ĳ�ʱʱ�䣬��select��timeoutʱ������������ʱʱ��֮�����¼������ͷ����ˣ������ڳ�ʱ�󲻹�����һ�����أ�����ֵͬ2
    retval = select(eventLoop->maxfd+1,
                &state->_rfds,&state->_wfds,NULL,tvp);
    if (retval > 0) { //���select�ķ���ֵ���������ж� �Ƿ����ļ� �ɶ����д�����������ĸ��ļ�����Ҫ�Լ�ȥ���
        for (j = 0; j <= eventLoop->maxfd; j++) {
            int mask = 0;
            aeFileEvent *fe = &eventLoop->events[j];

            if (fe->mask == AE_NONE) continue;
            if (fe->mask & AE_READABLE && FD_ISSET(j,&state->_rfds))
                mask |= AE_READABLE;
            if (fe->mask & AE_WRITABLE && FD_ISSET(j,&state->_wfds))
                mask |= AE_WRITABLE;
            eventLoop->fired[numevents].fd = j; //��¼������events[j]�Ѿ�����������������
            eventLoop->fired[numevents].mask = mask;
            numevents++;
        }
    }
    return numevents;//.���ص��ǣ��ж��ٸ�FileDescriptor�Ѿ�Ready�ˣ���Ҫ������
}

static char *aeApiName(void) {
    return "select";
}
