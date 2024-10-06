#include "MQTTWorker.h"
#include <QDebug>
#include <QStringList>
#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>

MQTTWorker::MQTTWorker(const char *address, const char *clientId, const char *topic, int qos, long timeout, QObject *parent)
    : QObject(parent),
      ADDRESS(address),
      CLIENTID(clientId),
      TOPIC(topic),
      QOS_LEVEL(qos),
      TIMEOUT_MS(timeout),
      running(true)
{
    MQTTClient_create(&client, ADDRESS.toUtf8().constData(), CLIENTID.toUtf8().constData(), MQTTCLIENT_PERSISTENCE_NONE, NULL);
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    int rc;
    if ((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS)
    {
        qDebug() << "无法连接到 MQTT 代理，返回代码：" << rc;
        running = false;
    }
}

MQTTWorker::~MQTTWorker()
{
    if (running)
    {
        stop();
    }
    MQTTClient_disconnect(client, 1000);
    MQTTClient_destroy(&client);
}

void MQTTWorker::publishMessage(const QString &message)
{
    if (!running)
        return;

    // 解析输入字符串
    QJsonObject jsonObj;
    QStringList pairs = message.split(',', Qt::SkipEmptyParts);
    for (const QString &pair : pairs)
    {
        QStringList keyValue = pair.split(':', Qt::KeepEmptyParts);
        if (keyValue.size() != 2)
        {
            qDebug() << "无效的键值对：" << pair;
            continue; // 跳过无效的键值对
        }
        QString key = keyValue.at(0).trimmed();
        QString value = keyValue.at(1).trimmed();

        if (key == "sample_id" || key == "user_uuid")
        {
            // 处理字符串类型的字段
            jsonObj.insert(key, value);
        }
        else if (key == "data")
        {
            // 处理 data 字段，将其转换为整数
            bool ok;
            int intValue = value.toInt(&ok);
            if (ok)
            {
                jsonObj.insert(key, intValue);
            }
            else
            {
                qDebug() << "data 不是有效的整数：" << value;
                // 根据需求，可以选择跳过或设置默认值
                jsonObj.insert(key, 0); // 例如，设置为 0
            }
        }
        else if (key == "channel_id")
        {
            bool ok;
            int intValue = value.toInt(&ok);
            if (ok)
            {
                jsonObj.insert(key, intValue);
            }
            else
            {
                qDebug() << "channel_id 不是有效的整数：" << value;

                jsonObj.insert(key, 0); 
            }
        }
        else
        {
            
            qDebug() << "未知的键：" << key;

        }
    }

    // 将 QJsonObject 转换为 QString
    QJsonDocument jsonDoc(jsonObj);
    QString jsonString = QString::fromUtf8(jsonDoc.toJson(QJsonDocument::Compact));

    // 准备 MQTT 消息
    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    QByteArray byteArray = jsonString.toUtf8();
    pubmsg.payload = (void *)byteArray.constData();
    pubmsg.payloadlen = byteArray.size();
    pubmsg.qos = QOS_LEVEL;
    pubmsg.retained = 0;
    MQTTClient_deliveryToken token;

    // 发布 JSON 消息
    int rc = MQTTClient_publishMessage(client, TOPIC.toUtf8().constData(), &pubmsg, &token);
    if (rc != MQTTCLIENT_SUCCESS)
    {
        qDebug() << "发布消息失败，返回代码：" << rc;
        return;
    }

    // 可选：等待消息发送完成
    // rc = MQTTClient_waitForCompletion(client, token, TIMEOUT_MS);
    // if (rc != MQTTCLIENT_SUCCESS)
    // {
    //     qDebug() << "消息发送失败，返回代码：" << rc;
    // }
    // else
    // {
    //     qDebug() << "消息发布成功：" << jsonString;
    // }
}

void MQTTWorker::stop()
{
    running = false;
    emit finished();
}
