//
// Copyright (C) 2014 OpenSim Ltd.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program; if not, see <http://www.gnu.org/licenses/>.
//

#include "inet/physicallayer/ieee80211/errormodel/layered/Ieee80211OFDMErrorModel.h"
#include "inet/physicallayer/contract/IAPSKModulation.h"
#include "inet/physicallayer/modulation/BPSKModulation.h"
#include "inet/physicallayer/common/layered/SignalPacketModel.h"
#include "inet/physicallayer/common/layered/SignalBitModel.h"
#include "inet/physicallayer/common/layered/SignalSampleModel.h"
#include "inet/physicallayer/common/layered/SignalSymbolModel.h"
#include "inet/physicallayer/analogmodel/layered/ScalarSignalAnalogModel.h"
#include "inet/physicallayer/common/layered/LayeredTransmission.h"
#include "inet/physicallayer/ieee80211/layered/Ieee80211OFDMSymbolModel.h"

namespace inet {

namespace physicallayer {

Define_Module(Ieee80211OFDMErrorModel);

void Ieee80211OFDMErrorModel::initialize(int stage)
{
    if (stage == INITSTAGE_LOCAL) {
        dataSymbolErrorRate = par("dataSymbolErrorRate");
        dataBitErrorRate = par("dataBitErrorRate");
        signalSymbolErrorRate = par("signalSymbolErrorRate");
        signalBitErrorRate = par("signalBitErrorRate");
    }
}

const IReceptionBitModel *Ieee80211OFDMErrorModel::computeBitModel(const LayeredTransmission *transmission, const ISNIR *snir) const
{
    const ITransmissionBitModel *transmissionBitModel = transmission->getBitModel();
    int signalBitLength = transmissionBitModel->getHeaderBitLength();
    bps signalBitRate = transmissionBitModel->getHeaderBitRate();
    int dataBitLength = transmissionBitModel->getPayloadBitLength();
    bps dataBitRate = transmissionBitModel->getPayloadBitRate();
    ASSERT(transmission->getSymbolModel() != nullptr);
    const IModulation *signalModulation = transmission->getSymbolModel()->getHeaderModulation();
    const IModulation *dataModulation = transmission->getSymbolModel()->getPayloadModulation();
    const BitVector *bits = transmissionBitModel->getBits();
    BitVector *corruptedBits = new BitVector(*bits); // FIXME: memory leak
    const ScalarTransmissionSignalAnalogModel *analogModel = dynamic_cast<const ScalarTransmissionSignalAnalogModel *>(transmission->getAnalogModel());
    if (dynamic_cast<const IAPSKModulation *>(signalModulation)) {
        const IAPSKModulation *apskSignalModulation = (const IAPSKModulation *)signalModulation;
        double signalFieldBer = isNaN(signalBitErrorRate) ? apskSignalModulation->calculateBER(snir->getMin(), analogModel->getBandwidth(), signalBitRate) : signalBitErrorRate;
        corruptBits(corruptedBits, signalFieldBer, 0, signalBitLength);
    }
    else
        throw cRuntimeError("Unknown signal modulation");
    if (dynamic_cast<const IAPSKModulation *>(dataModulation)) {
        const IAPSKModulation *apskDataModulation = (const IAPSKModulation *)signalModulation;
        double dataFieldBer = isNaN(dataBitErrorRate) ? apskDataModulation->calculateBER(snir->getMin(), analogModel->getBandwidth(), dataBitRate) : dataBitErrorRate;
        corruptBits(corruptedBits, dataFieldBer, signalBitLength, corruptedBits->getSize());
    }
    else
        throw cRuntimeError("Unknown data modulation");
    return new const ReceptionBitModel(signalBitLength, signalBitRate, dataBitLength, dataBitRate, corruptedBits);
}

const IReceptionSymbolModel *Ieee80211OFDMErrorModel::computeSymbolModel(const LayeredTransmission *transmission, const ISNIR *snir) const
{
    const ITransmissionBitModel *transmissionBitModel = transmission->getBitModel();
    const TransmissionSymbolModel *transmissionSymbolModel = check_and_cast<const Ieee80211OFDMTransmissionSymbolModel *>(transmission->getSymbolModel());
    const IModulation *modulation = transmissionSymbolModel->getPayloadModulation();
    const APSKModulationBase *dataModulation = check_and_cast<const APSKModulationBase *>(modulation);
    unsigned int dataFieldConstellationSize = dataModulation->getConstellationSize();
    unsigned int signalFieldConstellationSize = BPSKModulation::singleton.getConstellationSize();
    const std::vector<APSKSymbol> *constellationForDataField = dataModulation->getConstellation();
    const std::vector<APSKSymbol> *constellationForSignalField = BPSKModulation::singleton.getConstellation();
    const ScalarTransmissionSignalAnalogModel *analogModel = dynamic_cast<const ScalarTransmissionSignalAnalogModel *>(transmission->getAnalogModel());
    double signalSER = isNaN(signalSymbolErrorRate) ? BPSKModulation::singleton.calculateSER(snir->getMin(), analogModel->getBandwidth(), transmissionBitModel->getHeaderBitRate()) : signalSymbolErrorRate;
    double dataSER = isNaN(dataSymbolErrorRate) ? dataModulation->calculateSER(snir->getMin(), analogModel->getBandwidth(), transmissionBitModel->getPayloadBitRate()) : dataSymbolErrorRate;
    const std::vector<const ISymbol *> *symbols = transmissionSymbolModel->getSymbols();
    std::vector<const ISymbol *> *corruptedSymbols = new std::vector<const ISymbol *>(); // FIXME: memory leak
    // Only the first symbol is signal field symbol
    corruptedSymbols->push_back(corruptOFDMSymbol(check_and_cast<const Ieee80211OFDMSymbol *>(symbols->at(0)), signalSER, signalFieldConstellationSize, constellationForSignalField));
    // The remaining are all data field symbols
    for (unsigned int i = 1; i < symbols->size(); i++) {
        Ieee80211OFDMSymbol *corruptedOFDMSymbol = corruptOFDMSymbol(check_and_cast<const Ieee80211OFDMSymbol *>(symbols->at(i)), dataSER,
                    dataFieldConstellationSize, constellationForDataField);
        corruptedSymbols->push_back(corruptedOFDMSymbol);
    }
    return new Ieee80211OFDMReceptionSymbolModel(transmissionSymbolModel->getHeaderSymbolLength(), transmissionSymbolModel->getHeaderSymbolRate(), transmissionSymbolModel->getPayloadSymbolLength(), transmissionSymbolModel->getPayloadSymbolRate(), corruptedSymbols);
}

void Ieee80211OFDMErrorModel::corruptBits(BitVector *bits, double ber, int begin, int end) const
{
    for (int i = begin; i != end; i++) {
        double p = uniform(0, 1);
        if (p <= ber)
            bits->toggleBit(i);
    }
}

Ieee80211OFDMSymbol *Ieee80211OFDMErrorModel::corruptOFDMSymbol(const Ieee80211OFDMSymbol *symbol, double ser, int constellationSize, const std::vector<APSKSymbol> *constellation) const
{
    std::vector<const APSKSymbol *> subcarrierSymbols = copySubcarriers(symbol->getSubCarrierSymbols());
    for (int j = 0; j < symbol->symbolSize(); j++) {
        double p = uniform(0, 1);
        if (p <= ser) {
            int corruptedSubcarrierSymbolIndex = intuniform(0, constellationSize - 1); // TODO: it can be equal to the current symbol
            const APSKSymbol *corruptedSubcarrierSymbol = &constellation->at(corruptedSubcarrierSymbolIndex);
            subcarrierSymbols[j] = corruptedSubcarrierSymbol;
        }
    }
    return new Ieee80211OFDMSymbol(subcarrierSymbols);
}

std::vector<const APSKSymbol *> Ieee80211OFDMErrorModel::copySubcarriers(const std::vector<const APSKSymbol *>& subcarrierSymbols) const
{
    std::vector<const APSKSymbol *> copiedSubcarrierSymbols = subcarrierSymbols;
    copiedSubcarrierSymbols[5] = new APSKSymbol(*subcarrierSymbols[5]);
    copiedSubcarrierSymbols[19] = new APSKSymbol(*subcarrierSymbols[19]);
    copiedSubcarrierSymbols[33] = new APSKSymbol(*subcarrierSymbols[33]);
    copiedSubcarrierSymbols[47] = new APSKSymbol(*subcarrierSymbols[47]);
    return copiedSubcarrierSymbols;
}

const IReceptionSampleModel *Ieee80211OFDMErrorModel::computeSampleModel(const LayeredTransmission *transmission, const ISNIR *snir) const
{
    throw cRuntimeError("Unimplemented!");
    // TODO: implement sample error model
    const ITransmissionSampleModel *transmissionSampleModel = transmission->getSampleModel();
    int sampleLength = transmissionSampleModel->getSampleLength();
    double sampleRate = transmissionSampleModel->getSampleRate();
    const std::vector<W> *samples = transmissionSampleModel->getSamples();
    W rssi = W(0); // TODO: error model
    return new const ReceptionSampleModel(sampleLength, sampleRate, samples, rssi);
}

const IReceptionPacketModel *Ieee80211OFDMErrorModel::computePacketModel(const LayeredTransmission *transmission, const ISNIR *snir) const
{
    throw cRuntimeError("Unimplemented!");
    // TODO: implement error model
    const ITransmissionPacketModel *transmissionPacketModel = transmission->getPacketModel();
    const cPacket *packet = transmissionPacketModel->getPacket();
    double per = 0.0;
    bool packetErrorless = per == 0.0;
    return new const ReceptionPacketModel(packet, nullptr, bps(NaN), per, packetErrorless);
}
} /* namespace physicallayer */
} /* namespace inet */

