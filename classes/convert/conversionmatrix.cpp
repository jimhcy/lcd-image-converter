/*
 * LCD Image Converter. Converts images and fonts for embedded applciations.
 * Copyright (C) 2012 riuson
 * mailto: riuson@gmail.com
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/
 */

#include "conversionmatrix.h"
//-----------------------------------------------------------------------------
#include <QSettings>
#include <QVector>
#include <QStringList>
#include "conversionmatrixoptions.h"
//-----------------------------------------------------------------------------
ConversionMatrix::ConversionMatrix(QObject *parent) :
    QObject(parent)
{
    this->mData = new QVector<quint32>();
    for (int i = 0; i < ParamsItemsCount; i++)
        this->mData->append(0);

    this->mOptions = new ConversionMatrixOptions(this->mData, this);
    this->connect(this->mOptions, SIGNAL(changed()), SIGNAL(changed()));

    // initialize by default
    this->initMono(MonochromeTypeDiffuseDither);
}
//-----------------------------------------------------------------------------
ConversionMatrix::~ConversionMatrix()
{
    delete this->mOptions;
    delete this->mData;
}
//-----------------------------------------------------------------------------
ConversionMatrixOptions *ConversionMatrix::options() const
{
    return this->mOptions;
}
//-----------------------------------------------------------------------------
int ConversionMatrix::operationsCount() const
{
    return (this->mData->size() - ParamsItemsCount) >> 1;
}
//-----------------------------------------------------------------------------
void ConversionMatrix::operation(int index, quint32 *mask, int *shift, bool *left) const
{
    *mask = 0;
    *shift = 0;
    *left = false;

    if (index < this->operationsCount())
    {
        index = (index << 1) + ParamsItemsCount;

        *mask = this->mData->at(index);
        *shift = (this->mData->at(index + 1) & 0x0000001f);
        *left = (this->mData->at(index + 1) & 0x80000000) != 0;
    }
}
//-----------------------------------------------------------------------------
void ConversionMatrix::operationAdd(quint32 mask, int shift, bool left)
{
    this->mData->append(mask);
    if (left)
        this->mData->append(shift | 0x80000000);
    else
        this->mData->append(shift);

    emit this->changed();
}
//-----------------------------------------------------------------------------
void ConversionMatrix::operationRemove(int index)
{
    if (index < this->operationsCount())
    {
        index = (index << 1) + ParamsItemsCount;
        this->mData->remove(index + 1);
        this->mData->remove(index);
    }

    emit this->changed();
}
//-----------------------------------------------------------------------------
void ConversionMatrix::operationsRemoveAll()
{
    for (int i = this->operationsCount() - 1; i >= 0; i--)
        this->operationRemove(i);

    emit this->changed();
}
//-----------------------------------------------------------------------------
void ConversionMatrix::operationReplace(int index, quint32 mask, int shift, bool left)
{
    if (index < this->operationsCount())
    {
        index = (index << 1) + ParamsItemsCount;

        this->mData->replace(index, mask);

        if (left)
            this->mData->replace(index + 1, shift | 0x80000000);
        else
            this->mData->replace(index + 1, shift);
    }

    emit this->changed();
}
//-----------------------------------------------------------------------------
void ConversionMatrix::init(quint32 flags, quint32 maskUsed, quint32 maskAnd, quint32 maskOr, quint32 maskFill)
{
    this->mData->replace(0, flags);
    this->mData->replace(1, maskUsed);
    this->mData->replace(2, maskAnd);
    this->mData->replace(3, maskOr);
    this->mData->replace(4, maskFill);

    emit this->changed();
}
//-----------------------------------------------------------------------------
void ConversionMatrix::initMono(MonochromeType type, int edge)
{
    this->operationsRemoveAll();

    this->mOptions->setConvType(ConversionTypeMonochrome);
    this->mOptions->setMonoType(type);
    this->mOptions->setEdge(edge);
    this->mOptions->setBlockSize(Data8);
    this->mOptions->setMaskUsed(0x00000001);
    this->mOptions->setMaskAnd(0xffffffff);
    this->mOptions->setMaskOr(0x00000000);
    this->mOptions->setMaskFill(0x000000ff);

    // bits shift
    {
        this->mData->append(0x00000001);

        this->mData->append(0x00000000);
    }

    emit this->changed();
}
//-----------------------------------------------------------------------------
void ConversionMatrix::initGrayscale(int bits)
{
    this->operationsRemoveAll();

    if (bits > 8) bits = 8;
    if (bits < 1) bits = 1;

    this->mOptions->setConvType(ConversionTypeGrayscale);
    this->mOptions->setBlockSize(Data16);

    // mask of used bits before packing
    {
        quint32 mask = 0x000000ff;
        mask = mask << bits;
        mask = mask >> 8;
        mask = mask & 0x000000ff;

        this->mOptions->setMaskUsed(mask);
    }

    this->mOptions->setMaskAnd(0xffffffff);
    this->mOptions->setMaskOr(0x00000000);
    this->mOptions->setMaskFill(0x0000ffff);

    // bits shift
    {
        quint32 mask = 0x0000ff00;
        mask = mask >> bits;
        mask = mask & 0x000000ff;
        this->mData->append(mask);

        quint32 shift = 8 - bits;
        this->mData->append(shift);
    }

    emit this->changed();
}
//-----------------------------------------------------------------------------
void ConversionMatrix::initColor(int redBits, int greenBits, int blueBits)
{
    this->operationsRemoveAll();

    if (redBits > 8) redBits = 8;
    if (redBits < 1) redBits = 1;
    if (greenBits > 8) greenBits = 8;
    if (greenBits < 1) greenBits = 1;
    if (blueBits > 8) blueBits = 8;
    if (blueBits < 1) blueBits = 1;

    int bits = redBits + greenBits + blueBits;

    this->mOptions->setConvType(ConversionTypeColor);
    this->mOptions->setBlockSize(Data32);

    // mask of used bits before packing
    {
        quint64 mask64 = 0x00000000ffffffff;
        mask64 = mask64 << bits;
        mask64 = mask64 >> 32;
        mask64 = mask64 & 0x0000000000ffffff; // 24 bits
        quint32 mask = (quint32)mask64;
        this->mOptions->setMaskUsed(mask);
    }

    this->mOptions->setMaskAnd(0xffffffff);
    this->mOptions->setMaskOr(0x00000000);
    this->mOptions->setMaskFill(0xffffffff);

    // red bits shift
    {
        quint32 mask = 0x0000ff00;
        mask = mask >> redBits;
        mask = mask & 0x000000ff;
        mask = mask << 16;
        this->mData->append(mask);

        quint32 shift = 24 - bits;
        this->mData->append(shift);
    }

    // green bits shift
    {
        quint32 mask = 0x0000ff00;
        mask = mask >> greenBits;
        mask = mask & 0x000000ff;
        mask = mask << 8;
        this->mData->append(mask);

        quint32 shift = 16 - greenBits - blueBits;
        this->mData->append(shift);
    }

    // blue bits shift
    {
        quint32 mask = 0x0000ff00;
        mask = mask >> blueBits;
        mask = mask & 0x000000ff;
        this->mData->append(mask);

        quint32 shift = 8 - blueBits;
        this->mData->append(shift);
    }

    emit this->changed();
}
//-----------------------------------------------------------------------------
bool ConversionMatrix::load(const QString &name)
{
    if (name.isEmpty())
        return false;

    QSettings sett;
    sett.beginGroup("presets");

    if (sett.childGroups().contains(name))
    {
        sett.beginGroup(name);

        QString strFlags = sett.value("flags", QString("00000000")).toString();
        QString strMaskUsed = sett.value("maskUsed", QString("ffffffff")).toString();
        QString strMaskAnd = sett.value("maskAnd", QString("ffffffff")).toString();
        QString strMaskOr = sett.value("maskOr", QString("00000000")).toString();
        QString strMaskFill = sett.value("maskFill", QString("ffffffff")).toString();


        bool ok;
        quint32 flags, maskUsed, maskAnd, maskOr, maskFill;
        flags = strFlags.toUInt(&ok, 16);
        if (ok)
        {
            maskUsed = strMaskUsed.toUInt(&ok, 16);

            if (ok)
            {
                maskAnd = strMaskAnd.toUInt(&ok, 16);
                if (ok)
                {
                    maskOr = strMaskOr.toUInt(&ok, 16);

                    if (ok)
                    {
                        maskFill = strMaskFill.toUInt(&ok, 16);

                        if (ok)
                        {
                            this->mData->clear();
                            this->mData->append(flags);
                            this->mData->append(maskUsed);
                            this->mData->append(maskAnd);
                            this->mData->append(maskOr);
                            this->mData->append(maskFill);

                            int operations = sett.beginReadArray("matrix");

                            for (int i = 0; i < operations; i++)
                            {
                                sett.setArrayIndex(i);

                                QString strMask = sett.value("mask", QString("00000000")).toString();
                                QString strShift = sett.value("shift", QString("00000000")).toString();
                                quint32 mask, shift;
                                if (ok)
                                {
                                    mask = strMask.toUInt(&ok, 16);

                                    if (ok)
                                    {
                                        shift = strShift.toUInt(&ok, 16);
                                        if (ok)
                                        {
                                            this->mData->append(mask);
                                            this->mData->append(shift);
                                        }
                                    }
                                }
                            }
                        }
                    }

                    sett.endArray();;
                }

            }

        }

        sett.endGroup();
    }
    sett.endGroup();

    emit this->changed();

    return true;
}
//-----------------------------------------------------------------------------
bool ConversionMatrix::save(const QString &name) const
{
    QSettings sett;
    sett.beginGroup("presets");

    sett.beginGroup(name);
    sett.remove("");

    sett.setValue("flags", QString("%1").arg(this->mData->at(0), 8, 16, QChar('0')));
    sett.setValue("maskUsed", QString("%1").arg(this->mData->at(1), 8, 16, QChar('0')));
    sett.setValue("maskAnd", QString("%1").arg(this->mData->at(2), 8, 16, QChar('0')));
    sett.setValue("maskOr", QString("%1").arg(this->mData->at(3), 8, 16, QChar('0')));
    sett.setValue("maskFill", QString("%1").arg(this->mData->at(4), 8, 16, QChar('0')));

    sett.beginWriteArray("matrix");

    for (int i = 0; i < this->operationsCount(); i++)
    {
        sett.setArrayIndex(i);
        sett.setValue("mask", QString("%1").arg(this->mData->at(ParamsItemsCount + (i << 1)), 8, 16, QChar('0')));
        sett.setValue("shift", QString("%1").arg(this->mData->at(ParamsItemsCount + (i << 1) + 1), 8, 16, QChar('0')));
    }
    sett.endArray();

    sett.endGroup();
    sett.endGroup();

    return true;
}
//-----------------------------------------------------------------------------