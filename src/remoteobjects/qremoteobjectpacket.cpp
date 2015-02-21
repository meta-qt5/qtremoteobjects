/****************************************************************************
**
** Copyright (C) 2014 Ford Motor Company
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtRemoteObjects module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qremoteobjectpacket_p.h"

#include "qremoteobjectpendingcall.h"
#include "qremoteobjectsource.h"

#include "private/qmetaobjectbuilder_p.h"

QT_BEGIN_NAMESPACE

namespace QRemoteObjectPackets {

QRemoteObjectPacket::~QRemoteObjectPacket(){}

QRemoteObjectPacket *QRemoteObjectPacket::fromDataStream(QDataStream &in)
{
    QRemoteObjectPacket *packet = Q_NULLPTR;
    quint16 type;
    in >> type;
    switch (type) {
    case InitPacket:
        packet = new QInitPacket;
        if (packet->deserialize(in))
            packet->id = InitPacket;
        break;
    case InitDynamicPacket:
        packet = new QInitDynamicPacket;
        if (packet->deserialize(in))
            packet->id = InitDynamicPacket;
        break;
    case AddObject:
        packet = new QAddObjectPacket;
        if (packet->deserialize(in))
            packet->id = AddObject;
        break;
    case RemoveObject:
        packet = new QRemoveObjectPacket;
        if (packet->deserialize(in))
            packet->id = RemoveObject;
        break;
    case InvokePacket:
        packet = new QInvokePacket;
        if (packet->deserialize(in))
            packet->id = InvokePacket;
        break;
    case InvokeReplyPacket:
        packet = new QInvokeReplyPacket;
        if (packet->deserialize(in))
            packet->id = InvokeReplyPacket;
        break;
    case PropertyChangePacket:
        packet = new QPropertyChangePacket;
        if (packet->deserialize(in))
            packet->id = PropertyChangePacket;
        break;
    case ObjectList:
        packet = new QObjectListPacket;
        if (packet->deserialize(in))
            packet->id = ObjectList;
        break;
    default:
        qWarning() << "Invalid packet received" << type;
    }
    return packet;
}

QByteArray QInitPacketEncoder::serialize() const
{
    DataStreamPacket ds(id);
    ds << api->name();
    qint64 postNamePosition = ds.device()->pos();
    ds << quint32(0);

    //Now copy the property data
    const QMetaObject *meta = object->metaObject();

    const int numProperties = api->propertyCount();
    ds << quint32(numProperties);  //Number of properties

    for (int i = 0; i < numProperties; ++i) {
        const QMetaProperty mp = meta->property(api->sourcePropertyIndex(i));
        ds << mp.name();
        ds << mp.read(object);
    }

    //Now go back and set the size of the rest of the data so we can treat is as a QByteArray
    ds.device()->seek(postNamePosition);
    ds << quint32(ds.array.length() - sizeof(quint32) - postNamePosition);
    return ds.finishPacket();
}

bool QInitPacketEncoder::deserialize(QDataStream &)
{
    Q_ASSERT(false); //Use QInitPacket::deserialize()
    return false;
}

QByteArray QInitPacket::serialize() const
{
    Q_ASSERT(false); //Use QInitPacketEncoder::serialize()
    return QByteArray();
}

bool QInitPacket::deserialize(QDataStream& in)
{
    if (in.atEnd())
        return false;
    in >> name;
    if (name.isEmpty() || name.isNull() || in.atEnd())
        return false;
    in >> packetData;
    if (packetData.isEmpty() || packetData.isNull())
        return false;

    //Make sure the bytearray holds valid properties
    QDataStream validate(packetData);
    const int packetLen = packetData.size();
    quint32 nParam, len;
    quint8 c;
    QVariant tmp;
    validate >> nParam;
    for (quint32 i = 0; i < nParam; ++i)
    {
        const qint64 pos = validate.device()->pos();
        qint64 bytesLeft = packetLen - pos;
        if (bytesLeft < 4)
            return false;
        validate >> len;
        bytesLeft -= 4;
        if (bytesLeft < len)
            return false;
        validate.skipRawData(len-1);
        validate >> c;
        if (c != 0)
            return false;
        if (qstrlen(packetData.constData()+pos+4) != len - 1)
            return false;
        bytesLeft -= len;
        if (bytesLeft <= 0)
            return false;
        validate >> tmp;
        if (!tmp.isValid())
            return false;
    }
    return true;
}

QByteArray QInitDynamicPacketEncoder::serialize() const
{
    DataStreamPacket ds(id);
    ds << api->name();
    qint64 postNamePosition = ds.device()->pos();
    ds << quint32(0);

    //Now copy the property data
    const QMetaObject *meta = object->metaObject();

    const int numSignals = api->signalCount();
    ds << quint32(numSignals);  //Number of signals
    const int numMethods = api->methodCount();
    ds << quint32(numMethods);  //Number of methods

    for (int i = 0; i < numSignals; ++i) {
        ds << api->signalSignature(i);
        const int n = api->signalParameterCount(i);
        ds << quint32(n);
        for (int j = 0; j < n; ++j)
            ds << QVariant::typeToName(api->signalParameterType(i, j));
    }

    for (int i = 0; i < numMethods; ++i) {
        ds << api->methodSignature(i);
        ds << api->typeName(i);
        const int n = api->methodParameterCount(i);
        ds << quint32(n);
        for (int j = 0; j < n; ++j)
            ds << QVariant::typeToName(api->methodParameterType(i, j));
    }

    const int numProperties = api->propertyCount();
    ds << quint32(numProperties);  //Number of properties

    for (int i = 0; i < numProperties; ++i) {
        const QMetaProperty mp = meta->property(api->sourcePropertyIndex(i));
        ds << mp.name();
        ds << mp.typeName();
        if (mp.notifySignalIndex() == -1)
            ds << QByteArray();
        else
            ds << mp.notifySignal().methodSignature();
        ds << mp.read(object);
    }

    //Now go back and set the size of the rest of the data so we can treat is as a QByteArray
    ds.device()->seek(postNamePosition);
    ds << quint32(ds.array.length() - sizeof(quint32) - postNamePosition);
    return ds.finishPacket();
}

bool QInitDynamicPacketEncoder::deserialize(QDataStream &)
{
    Q_ASSERT(false); //Use QInitDynamicPacket::deserialize()
    return false;
}

QByteArray QInitDynamicPacket::serialize() const
{
    Q_ASSERT(false); //Use QInitDynamicPacketEncoder::serialize()
    return QByteArray();
}

bool QInitDynamicPacket::deserialize(QDataStream& in)
{
    if (in.atEnd())
        return false;
    in >> name;
    if (name.isEmpty() || name.isNull() || in.atEnd())
        return false;
    in >> packetData;
    if (packetData.isEmpty() || packetData.isNull())
        return false;

    //Make sure the bytearray holds valid properties // TODO maybe really evaluate
    return true;
    QDataStream validate(packetData);
    int packetLen = packetData.size();
    quint32 nParam, len, propLen;
    quint8 c;
    QVariant tmp;
    validate >> nParam;
    for (quint32 i = 0; i < nParam; ++i)
    {
        qint64 pos = validate.device()->pos();
        qint64 bytesLeft = packetLen - pos;
        if (bytesLeft < 4)
            return false;

        //Read property name
        validate >> len;
        bytesLeft -= 4;
        if (bytesLeft < len)
            return false;
        validate.skipRawData(len-1);
        validate >> c;
        if (c != 0)
            return false;
        if (qstrlen(packetData.constData()+pos+4) != len - 1)
            return false;
        bytesLeft -= len;
        if (bytesLeft <= 0)
            return false;

        //Read notify name
        propLen = len;
        validate >> len;
        bytesLeft -= 4;
        if (bytesLeft < len)
            return false;
        if (len) { //notify isn't empty
            validate.skipRawData(len-1);
            validate >> c;
            if (c != 0)
                return false;
            if (qstrlen(packetData.constData()+pos+4+propLen+4) != len - 1)
                return false;
            bytesLeft -= len;
        }
        if (bytesLeft <= 0)
            return false;

        //Read QVariant value
        validate >> tmp;
        if (!tmp.isValid())
            return false;
    }
    return true;
}

QMetaObject *QInitDynamicPacket::createMetaObject(QMetaObjectBuilder &builder,
                                                  int &outNumSignals,
                                                  QVector<bool> &methodReturnTypeIsVoid,
                                                  QVector<QVector<int> > &methodArgumentTypes,
                                                  QVector<QPair<QByteArray, QVariant> > *propertyValues) const
{
    quint32 numSignals = 0;
    quint32 numMethods = 0;
    quint32 numProperties = 0;

    methodArgumentTypes.clear();
    methodReturnTypeIsVoid.clear();

    QDataStream ds(packetData);
    ds >> numSignals;
    outNumSignals = numSignals;
    ds >> numMethods;

    methodArgumentTypes.resize(numSignals+numMethods);
    methodReturnTypeIsVoid.resize(numMethods);

    int curIndex = 0;

    for (quint32 i = 0; i < numSignals; ++i) {
        QByteArray signature, parameterType;
        quint32 parameterCount = 0;

        ds >> signature;
        ds >> parameterCount;
        methodArgumentTypes[curIndex].reserve(parameterCount);
        for (quint32 pCount = 0; pCount < parameterCount; ++pCount) {
            ds >> parameterType;
            methodArgumentTypes[curIndex] << QVariant::nameToType(parameterType.constData());
        }
        ++curIndex;
        builder.addSignal(signature);
    }

    for (quint32 i = 0; i < numMethods; ++i) {
        QByteArray signature, returnType, parameterType;
        quint32 parameterCount = 0;

        ds >> signature;
        ds >> returnType;
        ds >> parameterCount;
        methodArgumentTypes[curIndex].reserve(parameterCount);
        for (quint32 pCount = 0; pCount < parameterCount; ++pCount) {
            ds >> parameterType;
            methodArgumentTypes[curIndex] << QVariant::nameToType(parameterType.constData());
        }
        ++curIndex;
        const bool isVoid = returnType.isEmpty() || returnType == QByteArrayLiteral("void");
        methodReturnTypeIsVoid[i] = isVoid;
        if (isVoid)
            builder.addMethod(signature);
        else
            builder.addMethod(signature, QByteArrayLiteral("QRemoteObjectPendingCall"));
    }

    ds >> numProperties;

    QVector<QPair<QByteArray, QVariant> > &propVal = *propertyValues;
    for (quint32 i = 0; i < numProperties; ++i) {
        QByteArray name;
        QByteArray typeName;
        QByteArray signalName;
        ds >> name;
        ds >> typeName;
        ds >> signalName;
        if (signalName.isEmpty())
            builder.addProperty(name, typeName);
        else
            builder.addProperty(name, typeName, builder.indexOfSignal(signalName));
        QVariant value;
        ds >> value;
        propVal.append(qMakePair(name, value));
    }

    return builder.toMetaObject();
}


QByteArray QAddObjectPacket::serialize() const
{
    DataStreamPacket ds(id);
    ds << name;
    ds << isDynamic;
    return ds.finishPacket();
}

bool QAddObjectPacket::deserialize(QDataStream& in)
{
    in >> name;
    in >> isDynamic;
    return true;
}

QByteArray QRemoveObjectPacket::serialize() const
{
    DataStreamPacket ds(id);
    ds << name;
    return ds.finishPacket();
}

bool QRemoveObjectPacket::deserialize(QDataStream& in)
{
    in >> name;
    return true;
}

QByteArray QInvokePacket::serialize() const
{
    DataStreamPacket ds(id);
    ds << name;
    ds << call;
    ds << index;
    ds << args;
    ds << serialId;
    return ds.finishPacket();
}

bool QInvokePacket::deserialize(QDataStream& in)
{
    in >> name;
    in >> call;
    in >> index;
    in >> args;
    in >> serialId;
    return true;
}

QByteArray QInvokeReplyPacket::serialize() const
{
    DataStreamPacket ds(id);
    ds << name;
    ds << ackedSerialId;
    ds << value;
    return ds.finishPacket();
}

bool QInvokeReplyPacket::deserialize(QDataStream& in)
{
    in >> name;
    in >> ackedSerialId;
    in >> value;
    return true;
}

QByteArray QPropertyChangePacket::serialize() const
{
    DataStreamPacket ds(id);
    ds << name;
    ds << propertyName;
    ds << value;
    return ds.finishPacket();
}

bool QPropertyChangePacket::deserialize(QDataStream& in)
{
    in >> name;
    in >> propertyName;
    in >> value;
    return true;
}

QByteArray QObjectListPacket::serialize() const
{
    DataStreamPacket ds(id);
    ds << objects;
    return ds.finishPacket();
}

bool QObjectListPacket::deserialize(QDataStream& in)
{
    in >> objects;
    return true;
}

} // namespace QRemoteObjectPackets

QT_END_NAMESPACE