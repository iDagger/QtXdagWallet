#include "XdagWalletProcessThread.h"


XdagWalletProcessThread::XdagWalletProcessThread(QObject *parent = 0)
    :m_bNeedStopped(false)
{
    qDebug() << "xdag process thread constructor: " << QThread::currentThreadId();
}

XdagWalletProcessThread::~XdagWalletProcessThread()
{
    stop();
    quit();
    wait();
}

void XdagWalletProcessThread::setPoolAddr(const char* poolAddr)
{
    this->mPoolAddr = QString(poolAddr);
}

const char* XdagWalletProcessThread::getPoolAddr()
{
    return this->mPoolAddr.toStdString().c_str();
}

void XdagWalletProcessThread::setMutex(QMutex *mutex){
    this->m_pMutex = mutex;
}
void XdagWalletProcessThread::setCondPwdTyped(QWaitCondition *cond){
    this->m_pCondPwdTyped = cond;
}

void XdagWalletProcessThread::setCondPwdSeted(QWaitCondition *cond)
{
    this->m_pCondPwdSeted = cond;
}
void XdagWalletProcessThread::setCondPwdReTyped(QWaitCondition *cond){
    this->m_pCondPwdReTyped = cond;
}
void XdagWalletProcessThread::setCondRdmTyped(QWaitCondition *cond){
    this->m_pCondRdmTyped = cond;
}

void XdagWalletProcessThread::setCondUiNotified(QWaitCondition *cond)
{
    this->m_pUiNotified = cond;
}

QMutex* XdagWalletProcessThread::getMutex(void){
    return this->m_pMutex;
}

void XdagWalletProcessThread::setMsgMap(QMap<QString, QString> *map)
{
    this->m_pMsgMap = map;
}

QMap<QString, QString>* XdagWalletProcessThread::getMsgMap()
{
    return this->m_pMsgMap;
}

QWaitCondition* XdagWalletProcessThread::getCondPwdTyped(void){
    return this->m_pCondPwdTyped;
}

QWaitCondition *XdagWalletProcessThread::getCondPwdSeted()
{
    return this->m_pCondPwdSeted;
}
QWaitCondition* XdagWalletProcessThread::getCondPwdReTyped(void){
    return this->m_pCondPwdReTyped;
}
QWaitCondition *XdagWalletProcessThread::getCondRdmTyped(){
    return this->m_pCondRdmTyped;
}

QWaitCondition *XdagWalletProcessThread::getCondUiNotified()
{
    return this->m_pUiNotified;
}

void XdagWalletProcessThread::setMsgQueue(QQueue<UiNotifyMessage> *msgQueue)
{
    this->m_pMsgQueue = msgQueue;
}

void XdagWalletProcessThread::stop()
{
    qDebug() << "xdag process stop thread : " << QThread::currentThreadId();
    QMutexLocker locker(m_pMutex);
    m_bNeedStopped = true;
}

void XdagWalletProcessThread::waitPasswdTyped()
{
    qDebug() << " qcondiction wait password typed current thread id " << QThread::currentThreadId();
    this->m_pCondPwdTyped->wait(this->m_pMutex);
}

void XdagWalletProcessThread::wakePasswdTyped()
{
    qDebug() << " qcondiction wake password typed current thread id " << QThread::currentThreadId();
    this->m_pCondPwdTyped->wakeAll();
}

void XdagWalletProcessThread::waitPasswdSeted()
{
    this->m_pCondPwdSeted->wait(this->m_pMutex);
}

void XdagWalletProcessThread::wakePasswdSeted()
{
    this->m_pCondPwdSeted->wakeAll();
}

void XdagWalletProcessThread::waitPasswdRetyped()
{
    this->m_pCondPwdReTyped->wait(this->m_pMutex);
}

void XdagWalletProcessThread::wakePasswdRetyped()
{
     this->m_pCondPwdReTyped->wakeAll();
}

void XdagWalletProcessThread::waitRdmTyped()
{
    this->m_pCondRdmTyped->wait(this->m_pMutex);
}

void XdagWalletProcessThread::wakeRdmTyped()
{
    this->m_pCondRdmTyped->wakeAll();
}

void XdagWalletProcessThread::run()
{
    qDebug() << "xdag process thread run thread id: " << QThread::currentThreadId();
    m_bNeedStopped = false;

    /* dump the pool address and keep it always in memory */
    char* address = strdup(mPoolAddr.toStdString().c_str());
    xdag_wrapper_log_init();
    xdag_wrapper_init((void*)this,XdagWalletProcessCallback);
    xdag_global_init();
    xdag_main(address);

    /* start the main loop of the xdag proccess thread */
    while(!isInterruptionRequested()){
        m_pMutex->lock();

        if(m_pMsgQueue->isEmpty()){
            qDebug() << " wallet process thread waiting for message " << QThread::currentThreadId();
            m_pUiNotified->wait(m_pMutex);
        }

        /* read the ui notify message and process the message*/
        if(isInterruptionRequested()){
            qDebug() << " wallet process thread interrupted by ui " << QThread::currentThreadId();

            /*uninit xdag wallet*/
            xdag_wrapper_uninit();
            break;
        }
        /* pop message from the queue and process the message */
        UiNotifyMessage msg = m_pMsgQueue->first();
        m_pMsgQueue->pop_front();

        qDebug() << " receive message from thread : " << msg.msgFromThreadId
                 << " transfer account " << msg.account
                 << " transfer num " << msg.amount
                 << " message type " << msg.msgType;

        processUiNotifyMessage(msg);

        m_pMutex->unlock();
    }
}

void XdagWalletProcessThread::emitUISignal(UpdateUiInfo info)
{
    emit XdagWalletProcessSignal(info);
}


st_xdag_app_msg* XdagWalletProcessThread::XdagWalletProcessCallback(const void *call_back_object, st_xdag_event* event){

    XdagWalletProcessThread *thread = (XdagWalletProcessThread*)call_back_object;
    QMutex *mutex = thread->getMutex();
    QWaitCondition *condPwdTyped = thread->getCondPwdTyped();
    QWaitCondition *condPwdSeted = thread->getCondPwdSeted();
    QWaitCondition *condPwdReTyped = thread->getCondPwdReTyped();
    QWaitCondition *condRdmTyped = thread->getCondRdmTyped();

    qDebug() << " xdag process callback current thread id " << QThread::currentThreadId();

    if(NULL == event){
        qDebug() << " event is NULL";
        return NULL;
    }

    st_xdag_app_msg *msg = NULL;
    UpdateUiInfo updateUiInfo;
    switch(event->event_type){

        case en_event_set_pwd:
            qDebug() << " event type need set password current threadid " << QThread::currentThreadId();

            //wait ui set password
            mutex->lock();
            updateUiInfo.event_type = event->event_type;
            updateUiInfo.procedure_type = event->procedure_type;
            thread->emitUISignal(updateUiInfo);
            thread->waitPasswdSeted();

            msg = xdag_malloc_app_msg();
            if(msg){
                msg->xdag_pwd = strdup(thread->getMsgMap()->find("set-passwd")->toStdString().c_str());
                thread->getMsgMap()->clear();
            }
            mutex->unlock();
        return msg;

        case en_event_retype_pwd:
            qDebug() << " event type need retype password current threadid " << QThread::currentThreadId();

            //wait ui retype password
            mutex->lock();
            updateUiInfo.event_type = event->event_type;
            updateUiInfo.procedure_type = event->procedure_type;
            thread->emitUISignal(updateUiInfo);
            thread->waitPasswdRetyped();

            msg = xdag_malloc_app_msg();
            if(msg){
                msg->xdag_retype_pwd = strdup(thread->getMsgMap()->find("retype-passwd")->toStdString().c_str());
                thread->getMsgMap()->clear();
            }
            mutex->unlock();

        return msg;

        case en_event_set_rdm:
            qDebug() << " event type need type rdm current threadid " << QThread::currentThreadId();

            //wait ui set random keys
            mutex->lock();
            updateUiInfo.event_type = event->event_type;
            updateUiInfo.procedure_type = event->procedure_type;
            thread->emitUISignal(updateUiInfo);
            thread->waitRdmTyped();
            msg = xdag_malloc_app_msg();
            if(msg){
                msg->xdag_rdm = strdup(thread->getMsgMap()->find("type-rdm")->toStdString().c_str());
                thread->getMsgMap()->clear();
            }

            mutex->unlock();
        return msg;

        case en_event_pwd_not_same:
            qDebug() << " event type password not same";
            mutex->lock();
            updateUiInfo.event_type = event->event_type;
            updateUiInfo.procedure_type = event->procedure_type;
            thread->emitUISignal(updateUiInfo);
            mutex->unlock();
        return msg;

        case en_event_pwd_error:
            qDebug() << " event password error";
            qDebug() << " event type password not same";
            mutex->lock();
            updateUiInfo.event_type = event->event_type;
            updateUiInfo.procedure_type = event->procedure_type;
            thread->emitUISignal(updateUiInfo);
            mutex->unlock();
        return msg;

        case en_event_update_state:
            //qDebug() << " update ui address: " << event->address <<" balance: " << event->balance << " state " << event->state;

            mutex->lock();
            updateUiInfo.event_type = event->event_type;
            updateUiInfo.procedure_type = event->procedure_type;
            updateUiInfo.address = QString(event->address);
            updateUiInfo.balance = QString(event->balance);
            updateUiInfo.state = QString(event->state);
            thread->emitUISignal(updateUiInfo);
            mutex->unlock();
        //update ui need nothing to return;
        return NULL;

        case en_event_open_dnetfile_error:
            qDebug() << " open dnet file error " << event->error_msg;

            mutex->lock();
            updateUiInfo.event_type = event->event_type;
            updateUiInfo.procedure_type = event->procedure_type;
            thread->emitUISignal(updateUiInfo);
            mutex->unlock();
        return NULL;

        case en_event_open_walletfile_error:
            qDebug() << " open dnet file error " << event->error_msg;

            mutex->lock();
            updateUiInfo.event_type = event->event_type;
            updateUiInfo.procedure_type = event->procedure_type;
            thread->emitUISignal(updateUiInfo);
            mutex->unlock();
        return NULL;

        case en_event_load_storage_error:
            qDebug() << " open dnet file error " << event->error_msg;

            mutex->lock();
            updateUiInfo.event_type = event->event_type;
            updateUiInfo.procedure_type = event->procedure_type;
            thread->emitUISignal(updateUiInfo);
            mutex->unlock();
        return NULL;

        case en_event_xdag_log_print:
            qDebug() << event->app_log_msg;
        return NULL;

        default:
            qDebug() << " unknow event type : " << event->event_type;
        return NULL;
    }

    return NULL;
}

void XdagWalletProcessThread::processUiNotifyMessage(UiNotifyMessage & msg)
{
    switch(msg.msgType){

        case UiNotifyMessage::EN_DO_XFER_XDAG:
            xdag_send_coin(msg.amount,msg.account);
        break;

        case UiNotifyMessage::EN_QUIT_XDAG:

        break;
    }
}


