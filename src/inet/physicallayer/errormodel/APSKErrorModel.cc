//
// Copyright (C) 2013 OpenSim Ltd.
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

#include "inet/physicallayer/errormodel/APSKErrorModel.h"
#include "inet/physicallayer/base/APSKModulationBase.h"
#include "inet/physicallayer/base/FlatTransmissionBase.h"

namespace inet {

namespace physicallayer {

Define_Module(APSKErrorModel);

void APSKErrorModel::printToStream(std::ostream& stream) const
{
    stream << "APSKErrorModel";
}

double APSKErrorModel::computePacketErrorRate(const ISNIR *snir) const
{
    double bitErrorRate = computeBitErrorRate(snir);
    if (bitErrorRate == 0.0)
        return 0.0;
    else if (bitErrorRate == 1.0)
        return 1.0;
    else {
        const FlatTransmissionBase *flatTransmission = check_and_cast<const FlatTransmissionBase *>(snir->getReception()->getTransmission());
        return 1.0 - pow(1.0 - bitErrorRate, flatTransmission->getHeaderBitLength() + flatTransmission->getPayloadBitLength());
    }
}

double APSKErrorModel::computeBitErrorRate(const ISNIR *snir) const
{
    const FlatTransmissionBase *flatTransmission = check_and_cast<const FlatTransmissionBase *>(snir->getReception()->getTransmission());
    const APSKModulationBase *modulation = dynamic_cast<const APSKModulationBase *>(flatTransmission->getModulation());
    return modulation->calculateBER(snir->getMin(), flatTransmission->getBandwidth(), flatTransmission->getBitrate());
}

double APSKErrorModel::computeSymbolErrorRate(const ISNIR *snir) const
{
    const FlatTransmissionBase *flatTransmission = check_and_cast<const FlatTransmissionBase *>(snir->getReception()->getTransmission());
    const APSKModulationBase *modulation = dynamic_cast<const APSKModulationBase *>(flatTransmission->getModulation());
    return modulation->calculateSER(snir->getMin(), flatTransmission->getBandwidth(), flatTransmission->getBitrate());
}

} // namespace physicallayer

} // namespace inet

