/*
 * Copyright (c) 2014-2015, Intel Corporation
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation and/or
 * other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, Value, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "FloatingPointParameterType.h"
#include <sstream>
#include <iomanip>
#include "ParameterAccessContext.h"
#include "ConfigurationAccessContext.h"
#include <limits>
#include <climits>
#include "convert.hpp"
#include "Utility.h"

using std::string;

CFloatingPointParameterType::CFloatingPointParameterType(const string& strName)
    : base(strName),
      _fMin(-std::numeric_limits<float>::max()),
      _fMax(std::numeric_limits<float>::max())
{
}

string CFloatingPointParameterType::getKind() const
{
    return "FloatingPointParameter";
}

// Element properties
void CFloatingPointParameterType::showProperties(string& strResult) const
{
    base::showProperties(strResult);

    strResult += "Min:" + CUtility::toString(_fMin) + "\n" +
                 "Max:" + CUtility::toString(_fMax) + "\n";
}

void CFloatingPointParameterType::handleValueSpaceAttribute(
        CXmlElement& xmlConfigurableElementSettingsElement,
        CConfigurationAccessContext& configurationAccessContext) const
{
    if (!configurationAccessContext.serializeOut()) {

        string strValueSpace;

        if (xmlConfigurableElementSettingsElement.getAttribute("ValueSpace", strValueSpace)) {

            configurationAccessContext.setValueSpaceRaw(strValueSpace == "Raw");
        } else {

            configurationAccessContext.setValueSpaceRaw(false);
        }
    } else {
        // Set the value space only if it is raw (i.e. not the default one)
        if (configurationAccessContext.valueSpaceIsRaw()) {

            xmlConfigurableElementSettingsElement.setAttribute("ValueSpace", "Raw");
        }
    }
}

bool CFloatingPointParameterType::fromXml(const CXmlElement& xmlElement,
                                          CXmlSerializingContext& serializingContext)
{
    // Size. The XSD fixes it to 32
    uint32_t uiSizeInBits = 32;
    xmlElement.getAttribute("Size", uiSizeInBits);

    // Size support check: only floats are supported
    // (e.g. doubles are not supported)
    if (uiSizeInBits != sizeof(float) * CHAR_BIT) {

        serializingContext.setError("Unsupported size (" + CUtility::toString(uiSizeInBits) +
            ") for " + getKind() + " " + xmlElement.getPath() + ". For now, only 32 is supported.");

        return false;
    }

    setSize(uiSizeInBits / CHAR_BIT);

    xmlElement.getAttribute("Min", _fMin);
    xmlElement.getAttribute("Max", _fMax);

    if (_fMin > _fMax) {
        serializingContext.setError("Min (" + CUtility::toString(_fMin) +
                ") can't be greater than Max (" + CUtility::toString(_fMax) + ")");
        return false;
    }

    return base::fromXml(xmlElement, serializingContext);
}

bool CFloatingPointParameterType::toBlackboard(
        const string& strValue,
        uint32_t& uiValue,
        CParameterAccessContext& parameterAccessContext) const
{
    // Check Value integrity
    if (CUtility::isHexadecimal(strValue) && !parameterAccessContext.valueSpaceIsRaw()) {

        parameterAccessContext.setError("Hexadecimal values are not supported for "
                                        + getKind()
                                        + " when selected value space is real: "
                                        + strValue);

        return false;
    }

    if (parameterAccessContext.valueSpaceIsRaw()) {

        uint32_t uiData;

        // Raw value: interpret the user input as the memory content of the
        // parameter
        if (!convertTo(strValue, uiData)) {

            parameterAccessContext.setError("Value '" + strValue + "' is invalid");
            return false;
        }

        float fData = reinterpret_cast<const float&>(uiData);

        // Check against NaN or infinity
        if (!std::isfinite(fData)) {

            parameterAccessContext.setError("Value " + strValue + " is not a finite number");
            return false;
        }

        if (!checkValueAgainstRange(fData)) {

            setOutOfRangeError(strValue, parameterAccessContext);
            return false;
        }

        uiValue = uiData;
        return true;
    }
    else {

        float fValue;

        // Interpret the user input as float
        if (!convertTo(strValue, fValue)) {

            parameterAccessContext.setError("Value " + strValue + " is invalid");
            return false;
        }

        if (!checkValueAgainstRange(fValue)) {

            setOutOfRangeError(strValue, parameterAccessContext);
            return false;
        }

        // Move to the "raw memory" value space
        uiValue = reinterpret_cast<const uint32_t&>(fValue);
        return true;
    }
}

void CFloatingPointParameterType::setOutOfRangeError(
        const string& strValue,
        CParameterAccessContext& parameterAccessContext) const
{
    // error message buffer
    std::ostringstream ostrStream;

    ostrStream << "Value " << strValue << " standing out of admitted ";

    if (!parameterAccessContext.valueSpaceIsRaw()) {

        ostrStream << "real range [" << _fMin << ", " << _fMax << "]";
    } else {

        uint32_t uiMin = reinterpret_cast<const uint32_t&>(_fMin);
        uint32_t uiMax = reinterpret_cast<const uint32_t&>(_fMax);

        if (CUtility::isHexadecimal(strValue)) {

            ostrStream << std::showbase << std::hex
                       << std::setw(getSize() * 2) << std::setfill('0');
        }

        ostrStream << "raw range [" << uiMin << ", " << uiMax << "]";
    }
    ostrStream << " for " << getKind();

    parameterAccessContext.setError(ostrStream.str());
}

bool CFloatingPointParameterType::fromBlackboard(
        string& strValue,
        const uint32_t& uiValue,
        CParameterAccessContext& parameterAccessContext) const
{
    std::ostringstream ostrStream;

    if (parameterAccessContext.valueSpaceIsRaw()) {

        if (parameterAccessContext.outputRawFormatIsHex()) {

            ostrStream << std::showbase << std::hex
                       << std::setw(getSize() * 2) << std::setfill('0');
        }

        ostrStream << uiValue;
    } else {

        // Move from "raw memory" value space to real space
        float fValue = reinterpret_cast<const float&>(uiValue);

        ostrStream << fValue;
    }

    strValue = ostrStream.str();

    return true;
}

// Value access
bool CFloatingPointParameterType::toBlackboard(
        double dUserValue,
        uint32_t& uiValue,
        CParameterAccessContext& parameterAccessContext) const
{
    if (!checkValueAgainstRange(dUserValue)) {

        parameterAccessContext.setError("Value out of range");
        return false;
    }

    // Cast is fine because dValue has been checked against the value range
    float fValue = static_cast<float>(dUserValue);

    uiValue = reinterpret_cast<const uint32_t&>(fValue);
    return true;
}

bool CFloatingPointParameterType::fromBlackboard(
        double& dUserValue,
        uint32_t uiValue,
        CParameterAccessContext& parameterAccessContext) const
{
    (void) parameterAccessContext;

    // Move from "raw memory" value space to real space
    float fValue = reinterpret_cast<const float&>(uiValue);

    dUserValue = fValue;
    return true;
}

bool CFloatingPointParameterType::checkValueAgainstRange(double dValue) const
{
    // Check that dValue can safely be cast to a float
    // (otherwise, behaviour is undefined)
    if ((dValue < -std::numeric_limits<float>::max())
        || (dValue > std::numeric_limits<float>::max())) {
            return false;
    }

    return checkValueAgainstRange(static_cast<float>(dValue));
}

bool CFloatingPointParameterType::checkValueAgainstRange(float fValue) const
{
    return fValue <= _fMax && fValue >= _fMin;
}

void CFloatingPointParameterType::toXml(CXmlElement& xmlElement,
                                        CXmlSerializingContext& serializingContext) const
{
    xmlElement.setAttribute("Size", getSize() * CHAR_BIT);
    xmlElement.setAttribute("Min", _fMin);
    xmlElement.setAttribute("Max", _fMax);

    base::toXml(xmlElement, serializingContext);
}
