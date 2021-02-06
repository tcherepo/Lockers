#ifndef COMMUNICATOR_H
#define COMMUNICATOR_H

#include <QObject>
#include <QSerialPort>

class Communicator : public QObject
{
    Q_OBJECT
public:
    explicit Communicator(QObject *parent = nullptr);

    Q_INVOKABLE void setBoard(uchar board);
    Q_INVOKABLE void setChannel(uchar channel);

    Q_INVOKABLE void unlock();
    void poll();

signals:
    void lockLocked();
    void portError();

private slots :
    void Reconnect();
    void onReadyRead();
    void onErrorOccurred(QSerialPort::SerialPortError error);
    void onPortError();

private:
    QByteArray composeUnlockMessage();
    QByteArray composeStatusMesssage();

    bool verifyResponse(QByteArray response);
    void retryCommand();

    uchar m_board;
    uchar m_channel;

    static QByteArray header;
    enum commands {
        unlockcommand = 0x82,
        statuscommand = 0x83
    };

    static QString defaultPort;
    QSerialPort m_port;

    static int defaultReconnectms;
    static int defaultPollIntervalms;
    static int receiveBufferSize;

    enum states {
        undefined = 0x00,
        unlocking = 0x01,
        waitingtounlock = 0x02,
        polling = 0x03
    } m_state;

    bool m_retryonreconnect;
};

#endif // COMMUNICATOR_H
