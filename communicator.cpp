#include <QDebug>
#include <QTimer>

#include "communicator.h"

QByteArray Communicator::header("\x57\x4B\x4C\x59");
QString Communicator::defaultPort("/dev/ttyUSB0");
int Communicator::defaultReconnectms = 10000;
int Communicator::defaultPollIntervalms = 10000;
int Communicator::receiveBufferSize = 32;

Communicator::Communicator(QObject *parent) : QObject(parent),
    m_board(0x01),
    m_channel(0x01),
    m_port(defaultPort),
    m_state(undefined),
    m_retryonreconnect(false)
{
    m_port.setBaudRate(QSerialPort::Baud9600);
    m_port.setDataBits(QSerialPort::Data8);
    m_port.setParity(QSerialPort::NoParity);
    m_port.setStopBits(QSerialPort::OneStop);
    m_port.setFlowControl(QSerialPort::NoFlowControl);

    connect(&m_port, &QSerialPort::readyRead, this, &Communicator::onReadyRead);
    connect(this, &Communicator::portError, this, &Communicator::onPortError);
}

void Communicator::setBoard(uchar board)
{
    m_board = board;
    qDebug() << "Board: " + QString(m_board);
}

void Communicator::setChannel(uchar channel)
{
    m_channel = channel;
    qDebug() << "Channel: " + QString(m_channel);
}

void Communicator::unlock()
{
    m_state = unlocking;
    m_retryonreconnect = true;
    Reconnect();
}

void Communicator::poll()
{
    m_state = polling;

    QByteArray pollmessage = composeStatusMesssage();
    m_port.write(pollmessage);
    if ( !m_port.flush() )
    {
        m_retryonreconnect = true;
        emit portError();
    }
}

void Communicator::Reconnect()
{
    if ( m_port.isOpen() ) m_port.close();

    if ( !m_port.open(QIODevice::ReadWrite) )
    {
        qDebug() << "Failed to open port: " << m_port.errorString();

        QTimer::singleShot(defaultReconnectms, this, &Communicator::Reconnect);
        return;
    }

    if ( m_retryonreconnect ) retryCommand();
}

void Communicator::onReadyRead()
{
    QByteArray response = m_port.read(receiveBufferSize);

    qDebug() << "Received: " << response;

    if ( !verifyResponse(response) )
    {
        qDebug() << "Response verification failed.";
        retryCommand();
        return;
    }

    uchar responseboard = response.at(5);
    uchar responsechannel = response.at(8);
    uchar responsecommand = response.at(6);
    uchar responsestatus = response.at(7);
    uchar responselockstatus = response.at(9);

    if ( responseboard != m_board || responsechannel != m_channel )
    {
        qDebug() << "Response board/channel mismatch.";
        retryCommand();
        return;
    }

    if ( (m_state == unlocking && responsecommand != commands::unlockcommand) ||
         (m_state == polling && responsecommand != commands::statuscommand) )

    {
        qDebug() << "Response command mismatch.";
        retryCommand();
        return;
    }

    if ( responsestatus != 0x00 )
    {
        qDebug() << "Response status error.";
        retryCommand();
        return;
    }

    if ( responselockstatus == 0x00 )
    {
        switch (m_state) {
        case unlocking:
            poll();
            break;
        case waitingtounlock:
            poll();
            break;
        case polling:
            QTimer::singleShot(defaultPollIntervalms, this, &Communicator::poll);
            break;
        default:
            // should not be reached
            return;
            break;
        }
    }
    else if ( responselockstatus == 0x01 )
    {
        QByteArray statusmessage = composeStatusMesssage();
        switch (m_state) {
        case unlocking:
            m_state = waitingtounlock;
            m_port.write(statusmessage);
            if ( !m_port.flush() )
            {
                m_retryonreconnect = true;
                emit portError();
            }
            break;
        case waitingtounlock:
            m_port.write(statusmessage);
            if ( !m_port.flush() )
            {
                m_retryonreconnect = true;
                emit portError();
            }
            break;
        case polling:
            emit lockLocked();
            m_port.close();
            m_state = undefined;
            break;
        default:
            // should not be reached
            return;
            break;
        }
    }
    else
    {
        qDebug() << "Unknown response lock status.";
        retryCommand();
    }

}

void Communicator::onPortError()
{
    qDebug() << "port error: " << m_port.errorString();
    m_retryonreconnect = true;
    Reconnect();
}

QByteArray Communicator::composeUnlockMessage()
{
    QByteArray message;

    message.append(header);
    message.append(0x09);
    message.append(m_board);
    message.append(commands::unlockcommand);
    message.append(m_channel);

    uchar check = 0x00;
    for ( QByteArray::const_iterator it = message.constBegin(); it < message.constEnd(); ++it )
    {
        check ^= *it;
    }
    message.append(check);

    qDebug() << "Unlock message: " << message;

    return message;
}

QByteArray Communicator::composeStatusMesssage()
{
    QByteArray message;

    message.append(header);
    message.append(0x09);
    message.append(m_board);
    message.append(commands::statuscommand);
    message.append(m_channel);

    uchar check = 0x00;
    for ( QByteArray::const_iterator it = message.constBegin(); it < message.constEnd(); ++it )
    {
        check ^= *it;
    }
    message.append(check);

    qDebug() << "Status message: " << message;

    return message;
}

bool Communicator::verifyResponse(QByteArray response)
{
    if ( !response.startsWith(header) ) return false;

    uchar check = 0x00;
    for ( int i = 0; i < response.size() - 1; ++i)
    {
        check ^= response.at(i);
    }

    return ((uchar)response.at(response.size() - 1) == check) ? true : false;
}

void Communicator::retryCommand()
{
    QByteArray unlockmessage = composeUnlockMessage();
    QByteArray pollmessage = composeStatusMesssage();
    switch (m_state) {
    case unlocking:
        m_port.write(unlockmessage);
        if ( !m_port.flush() )
        {
            m_retryonreconnect = true;
            emit portError();
        }
        break;
    case waitingtounlock:
        m_port.write(pollmessage);
        if ( !m_port.flush() )
        {
            m_retryonreconnect = true;
            emit portError();
        }
        break;
    case polling:
        m_port.write(pollmessage);
        if ( !m_port.flush() )
        {
            m_retryonreconnect = true;
            emit portError();
        }
        break;
    default:
        // should not be reached
        return;
        break;
    }
}
