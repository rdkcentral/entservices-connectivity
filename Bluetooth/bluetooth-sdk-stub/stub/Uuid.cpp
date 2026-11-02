/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2025 RDK Management
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/

#include <bluetooth/Uuid.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace {
// Base UUID all 16-bit "well-known" Bluetooth UUIDs are derived from.
constexpr std::array<uint8_t, 16> kBluetoothBaseUuidBytes = {
    0x00, 0x00, 0x00, 0x00,             // First 4 bytes
    0x00, 0x00,                         // Next 2 bytes
    0x10, 0x00,                         // Next 2 bytes
    0x80, 0x00,                         // Next 2 bytes
    0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB  // Last 6 bytes
};
}  // namespace

namespace bluetooth {

Uuid::Uuid(uint16_t id) {
  std::copy(kBluetoothBaseUuidBytes.begin(), kBluetoothBaseUuidBytes.end(), m_uuid);
  m_uuid[2] = static_cast<uint8_t>((id >> 8) & 0xff);
  m_uuid[3] = static_cast<uint8_t>(id & 0xff);
}

Uuid::Uuid(const std::string& id) {
  // Strict canonical form xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx; sscanf alone would accept loose input.
  bool valid = id.size() == 36;
  for (size_t i = 0; valid && i < id.size(); ++i) {
    if (i == 8 || i == 13 || i == 18 || i == 23) {
      valid = id[i] == '-';
    } else {
      valid = std::isxdigit(static_cast<unsigned char>(id[i])) != 0;
    }
  }
  if (!valid) {
    throw std::runtime_error("Invalid 128-bit uuid:" + id);
  }
  unsigned int bytes[16];
  const int matched = std::sscanf(id.c_str(),
      "%2x%2x%2x%2x-%2x%2x-%2x%2x-%2x%2x-%2x%2x%2x%2x%2x%2x",
      &bytes[0], &bytes[1], &bytes[2], &bytes[3], &bytes[4], &bytes[5], &bytes[6], &bytes[7],
      &bytes[8], &bytes[9], &bytes[10], &bytes[11], &bytes[12], &bytes[13], &bytes[14], &bytes[15]);
  if (matched != 16) {
    throw std::runtime_error("Invalid 128-bit uuid:" + id);
  }
  std::transform(std::begin(bytes), std::end(bytes), m_uuid,
                 [](unsigned int b) { return static_cast<uint8_t>(b); });
}

bool Uuid::isWellKnown() const {
  // Bytes 2-3 carry the 16-bit id; only the 0000xxxx-0000-1000-8000-00805F9B34FB tail must match.
  return m_uuid[0] == 0 && m_uuid[1] == 0 &&
         std::equal(kBluetoothBaseUuidBytes.begin() + 4, kBluetoothBaseUuidBytes.end(), m_uuid + 4);
}

std::string Uuid::toString(bool shortForm) const {
  if (isWellKnown() && shortForm) {
    const uint16_t wellKnownUuid = static_cast<uint16_t>((m_uuid[2] << 8) | m_uuid[3]);
    std::stringstream buff;
    buff << "0x" << std::hex << std::setw(4) << std::setfill('0') << wellKnownUuid;
    return buff.str();
  }

  char buff[37];
  std::snprintf(buff, sizeof(buff),
      "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
      m_uuid[0], m_uuid[1], m_uuid[2], m_uuid[3], m_uuid[4], m_uuid[5], m_uuid[6], m_uuid[7],
      m_uuid[8], m_uuid[9], m_uuid[10], m_uuid[11], m_uuid[12], m_uuid[13], m_uuid[14], m_uuid[15]);
  return std::string(buff);
}


const Uuid Uuid::Services::AlertNotificationService{0x1811};
const Uuid Uuid::Services::AudioInputControlService{0x1843};
const Uuid Uuid::Services::AudioStreamControlService{0x184e};
const Uuid Uuid::Services::AuthorizationControlService{0x183d};
const Uuid Uuid::Services::AutomationIOService{0x1815};
const Uuid Uuid::Services::BasicAudioAnnouncementService{0x1851};
const Uuid Uuid::Services::BatteryService{0x180f};
const Uuid Uuid::Services::BinarySensorService{0x183b};
const Uuid Uuid::Services::BloodPressureService{0x1810};
const Uuid Uuid::Services::BodyCompositionService{0x181b};
const Uuid Uuid::Services::BondManagementService{0x181e};
const Uuid Uuid::Services::BroadcastAudioAnnouncementService{0x1852};
const Uuid Uuid::Services::BroadcastAudioScanService{0x184f};
const Uuid Uuid::Services::CommonAudioService{0x1853};
const Uuid Uuid::Services::ConstantToneExtensionService{0x184a};
const Uuid Uuid::Services::ContinuousGlucoseMonitoringService{0x181f};
const Uuid Uuid::Services::CoordinatedSetIdentificationService{0x1846};
const Uuid Uuid::Services::CurrentTimeService{0x1805};
const Uuid Uuid::Services::CyclingPowerService{0x1818};
const Uuid Uuid::Services::CyclingSpeedandCadenceService{0x1816};
const Uuid Uuid::Services::DeviceInformationService{0x180a};
const Uuid Uuid::Services::DeviceTimeService{0x1847};
const Uuid Uuid::Services::ElapsedTimeService{0x183f};
const Uuid Uuid::Services::ElectronicShelfLabelService{0x1857};
const Uuid Uuid::Services::EmergencyConfigurationService{0x183c};
const Uuid Uuid::Services::EnvironmentalSensingService{0x181a};
const Uuid Uuid::Services::FitnessMachineService{0x1826};
const Uuid Uuid::Services::GAPService{0x1800};
const Uuid Uuid::Services::GATTService{0x1801};
const Uuid Uuid::Services::GamingAudioService{0x1858};
const Uuid Uuid::Services::GenericHealthSensorService{0x1840};
const Uuid Uuid::Services::GenericMediaControlService{0x1849};
const Uuid Uuid::Services::GenericTelephoneBearerService{0x184c};
const Uuid Uuid::Services::GlucoseService{0x1808};
const Uuid Uuid::Services::HTTPProxyService{0x1823};
const Uuid Uuid::Services::HealthThermometerService{0x1809};
const Uuid Uuid::Services::HearingAccessService{0x1854};
const Uuid Uuid::Services::HeartRateService{0x180d};
const Uuid Uuid::Services::HumanInterfaceDeviceService{0x1812};
const Uuid Uuid::Services::ImmediateAlertService{0x1802};
const Uuid Uuid::Services::IndoorPositioningService{0x1821};
const Uuid Uuid::Services::IndustrialMeasurementDeviceService{0x185a};
const Uuid Uuid::Services::InsulinDeliveryService{0x183a};
const Uuid Uuid::Services::InternetProtocolSupportService{0x1820};
const Uuid Uuid::Services::LinkLossService{0x1803};
const Uuid Uuid::Services::LocationandNavigationService{0x1819};
const Uuid Uuid::Services::MediaControlService{0x1848};
const Uuid Uuid::Services::MeshProvisioningService{0x1827};
const Uuid Uuid::Services::MeshProxyService{0x1828};
const Uuid Uuid::Services::MeshProxySolicitationService{0x1859};
const Uuid Uuid::Services::MicrophoneControlService{0x184d};
const Uuid Uuid::Services::NextDSTChangeService{0x1807};
const Uuid Uuid::Services::ObjectTransferService{0x1825};
const Uuid Uuid::Services::PhoneAlertStatusService{0x180e};
const Uuid Uuid::Services::PhysicalActivityMonitorService{0x183e};
const Uuid Uuid::Services::PublicBroadcastAnnouncementService{0x1856};
const Uuid Uuid::Services::PublishedAudioCapabilitiesService{0x1850};
const Uuid Uuid::Services::PulseOximeterService{0x1822};
const Uuid Uuid::Services::RangingService{0x185b};
const Uuid Uuid::Services::ReconnectionConfigurationService{0x1829};
const Uuid Uuid::Services::ReferenceTimeUpdateService{0x1806};
const Uuid Uuid::Services::RunningSpeedandCadenceService{0x1814};
const Uuid Uuid::Services::ScanParametersService{0x1813};
const Uuid Uuid::Services::TelephoneBearerService{0x184b};
const Uuid Uuid::Services::TelephonyandMediaAudioService{0x1855};
const Uuid Uuid::Services::TransportDiscoveryService{0x1824};
const Uuid Uuid::Services::TxPowerService{0x1804};
const Uuid Uuid::Services::UserDataService{0x181c};
const Uuid Uuid::Services::VolumeControlService{0x1844};
const Uuid Uuid::Services::VolumeOffsetControlService{0x1845};
const Uuid Uuid::Services::WeightScaleService{0x181d};
const Uuid Uuid::Characteristics::ACSControlPoint{0x2b33};
const Uuid Uuid::Characteristics::ACSDataIn{0x2b30};
const Uuid Uuid::Characteristics::ACSDataOutIndicate{0x2b32};
const Uuid Uuid::Characteristics::ACSDataOutNotify{0x2b31};
const Uuid Uuid::Characteristics::ACSStatus{0x2b2f};
const Uuid Uuid::Characteristics::APSyncKeyMaterial{0x2bf7};
const Uuid Uuid::Characteristics::ASEControlPoint{0x2bc6};
const Uuid Uuid::Characteristics::Acceleration{0x2c06};
const Uuid Uuid::Characteristics::ActivePresetIndex{0x2bdc};
const Uuid Uuid::Characteristics::ActivityGoal{0x2b4e};
const Uuid Uuid::Characteristics::AdvertisingConstantToneExtensionInterval{0x2bb1};
const Uuid Uuid::Characteristics::AdvertisingConstantToneExtensionMinimumLength{0x2bae};
const Uuid Uuid::Characteristics::AdvertisingConstantToneExtensionMinimumTransmitCount{0x2baf};
const Uuid Uuid::Characteristics::AdvertisingConstantToneExtensionPHY{0x2bb2};
const Uuid Uuid::Characteristics::AdvertisingConstantToneExtensionTransmitDuration{0x2bb0};
const Uuid Uuid::Characteristics::AerobicHeartRateLowerLimit{0x2a7e};
const Uuid Uuid::Characteristics::AerobicHeartRateUpperLimit{0x2a84};
const Uuid Uuid::Characteristics::AerobicThreshold{0x2a7f};
const Uuid Uuid::Characteristics::Age{0x2a80};
const Uuid Uuid::Characteristics::Aggregate{0x2a5a};
const Uuid Uuid::Characteristics::AlertCategoryID{0x2a43};
const Uuid Uuid::Characteristics::AlertCategoryIDBitMask{0x2a42};
const Uuid Uuid::Characteristics::AlertLevel{0x2a06};
const Uuid Uuid::Characteristics::AlertNotificationControlPoint{0x2a44};
const Uuid Uuid::Characteristics::AlertStatus{0x2a3f};
const Uuid Uuid::Characteristics::Altitude{0x2ab3};
const Uuid Uuid::Characteristics::AmmoniaConcentration{0x2bcf};
const Uuid Uuid::Characteristics::AnaerobicHeartRateLowerLimit{0x2a81};
const Uuid Uuid::Characteristics::AnaerobicHeartRateUpperLimit{0x2a82};
const Uuid Uuid::Characteristics::AnaerobicThreshold{0x2a83};
const Uuid Uuid::Characteristics::ApparentEnergy32{0x2b89};
const Uuid Uuid::Characteristics::ApparentPower{0x2b8a};
const Uuid Uuid::Characteristics::ApparentWindDirection{0x2a73};
const Uuid Uuid::Characteristics::ApparentWindSpeed{0x2a72};
const Uuid Uuid::Characteristics::Appearance{0x2a01};
const Uuid Uuid::Characteristics::AudioInputControlPoint{0x2b7b};
const Uuid Uuid::Characteristics::AudioInputDescription{0x2b7c};
const Uuid Uuid::Characteristics::AudioInputState{0x2b77};
const Uuid Uuid::Characteristics::AudioInputStatus{0x2b7a};
const Uuid Uuid::Characteristics::AudioInputType{0x2b79};
const Uuid Uuid::Characteristics::AudioLocation{0x2b81};
const Uuid Uuid::Characteristics::AudioOutputDescription{0x2b83};
const Uuid Uuid::Characteristics::AvailableAudioContexts{0x2bcd};
const Uuid Uuid::Characteristics::AverageCurrent{0x2ae0};
const Uuid Uuid::Characteristics::AverageVoltage{0x2ae1};
const Uuid Uuid::Characteristics::BGRFeatures{0x2c04};
const Uuid Uuid::Characteristics::BGSFeatures{0x2c03};
const Uuid Uuid::Characteristics::BR_EDRHandoverData{0x2b38};
const Uuid Uuid::Characteristics::BSSControlPoint{0x2b2b};
const Uuid Uuid::Characteristics::BSSResponse{0x2b2c};
const Uuid Uuid::Characteristics::BarometricPressureTrend{0x2aa3};
const Uuid Uuid::Characteristics::BatteryCriticalStatus{0x2be9};
const Uuid Uuid::Characteristics::BatteryEnergyStatus{0x2bf0};
const Uuid Uuid::Characteristics::BatteryHealthInformation{0x2beb};
const Uuid Uuid::Characteristics::BatteryHealthStatus{0x2bea};
const Uuid Uuid::Characteristics::BatteryInformation{0x2bec};
const Uuid Uuid::Characteristics::BatteryLevel{0x2a19};
const Uuid Uuid::Characteristics::BatteryLevelStatus{0x2bed};
const Uuid Uuid::Characteristics::BatteryTimeStatus{0x2bee};
const Uuid Uuid::Characteristics::BearerListCurrentCalls{0x2bb9};
const Uuid Uuid::Characteristics::BearerProviderName{0x2bb3};
const Uuid Uuid::Characteristics::BearerSignalStrength{0x2bb7};
const Uuid Uuid::Characteristics::BearerSignalStrengthReportingInterval{0x2bb8};
const Uuid Uuid::Characteristics::BearerTechnology{0x2bb5};
const Uuid Uuid::Characteristics::BearerUCI{0x2bb4};
const Uuid Uuid::Characteristics::BearerURISchemesSupportedList{0x2bb6};
const Uuid Uuid::Characteristics::BloodPressureFeature{0x2a49};
const Uuid Uuid::Characteristics::BloodPressureMeasurement{0x2a35};
const Uuid Uuid::Characteristics::BloodPressureRecord{0x2b36};
const Uuid Uuid::Characteristics::BluetoothSIGData{0x2b39};
const Uuid Uuid::Characteristics::BodyCompositionFeature{0x2a9b};
const Uuid Uuid::Characteristics::BodyCompositionMeasurement{0x2a9c};
const Uuid Uuid::Characteristics::BodySensorLocation{0x2a38};
const Uuid Uuid::Characteristics::BondManagementControlPoint{0x2aa4};
const Uuid Uuid::Characteristics::BondManagementFeature{0x2aa5};
const Uuid Uuid::Characteristics::Boolean{0x2ae2};
const Uuid Uuid::Characteristics::BootKeyboardInputReport{0x2a22};
const Uuid Uuid::Characteristics::BootKeyboardOutputReport{0x2a32};
const Uuid Uuid::Characteristics::BootMouseInputReport{0x2a33};
const Uuid Uuid::Characteristics::BroadcastAudioScanControlPoint{0x2bc7};
const Uuid Uuid::Characteristics::BroadcastReceiveState{0x2bc8};
const Uuid Uuid::Characteristics::CGMFeature{0x2aa8};
const Uuid Uuid::Characteristics::CGMMeasurement{0x2aa7};
const Uuid Uuid::Characteristics::CGMSessionRunTime{0x2aab};
const Uuid Uuid::Characteristics::CGMSessionStartTime{0x2aaa};
const Uuid Uuid::Characteristics::CGMSpecificOpsControlPoint{0x2aac};
const Uuid Uuid::Characteristics::CGMStatus{0x2aa9};
const Uuid Uuid::Characteristics::CIE13_3_1995ColorRenderingIndex{0x2ae7};
const Uuid Uuid::Characteristics::CO2_Concentration{0x2b8c};
const Uuid Uuid::Characteristics::CSCFeature{0x2a5c};
const Uuid Uuid::Characteristics::CSCMeasurement{0x2a5b};
const Uuid Uuid::Characteristics::CallControlPoint{0x2bbe};
const Uuid Uuid::Characteristics::CallControlPointOptionalOpcodes{0x2bbf};
const Uuid Uuid::Characteristics::CallFriendlyName{0x2bc2};
const Uuid Uuid::Characteristics::CallState{0x2bbd};
const Uuid Uuid::Characteristics::CaloricIntake{0x2b50};
const Uuid Uuid::Characteristics::CarbonMonoxideConcentration{0x2bd0};
const Uuid Uuid::Characteristics::CardioRespiratoryActivityInstantaneousData{0x2b3e};
const Uuid Uuid::Characteristics::CardioRespiratoryActivitySummaryData{0x2b3f};
const Uuid Uuid::Characteristics::CentralAddressResolution{0x2aa6};
const Uuid Uuid::Characteristics::ChromaticDistancefromPlanckian{0x2ae3};
const Uuid Uuid::Characteristics::ChromaticityCoordinate{0x2b1c};
const Uuid Uuid::Characteristics::ChromaticityCoordinates{0x2ae4};
const Uuid Uuid::Characteristics::ChromaticityTolerance{0x2ae6};
const Uuid Uuid::Characteristics::ChromaticityinCCTandDuvValues{0x2ae5};
const Uuid Uuid::Characteristics::ClientSupportedFeatures{0x2b29};
const Uuid Uuid::Characteristics::Coefficient{0x2ae8};
const Uuid Uuid::Characteristics::ConstantToneExtensionEnable{0x2bad};
const Uuid Uuid::Characteristics::ContentControlID{0x2bba};
const Uuid Uuid::Characteristics::CoordinatedSetSize{0x2b85};
const Uuid Uuid::Characteristics::CorrelatedColorTemperature{0x2ae9};
const Uuid Uuid::Characteristics::CosineoftheAngle{0x2b8d};
const Uuid Uuid::Characteristics::Count16{0x2aea};
const Uuid Uuid::Characteristics::Count24{0x2aeb};
const Uuid Uuid::Characteristics::CountryCode{0x2aec};
const Uuid Uuid::Characteristics::CrossTrainerData{0x2ace};
const Uuid Uuid::Characteristics::CurrentElapsedTime{0x2bf2};
const Uuid Uuid::Characteristics::CurrentGroupObjectID{0x2ba0};
const Uuid Uuid::Characteristics::CurrentTime{0x2a2b};
const Uuid Uuid::Characteristics::CurrentTrackObjectID{0x2b9d};
const Uuid Uuid::Characteristics::CurrentTrackSegmentsObjectID{0x2b9c};
const Uuid Uuid::Characteristics::CyclingPowerControlPoint{0x2a66};
const Uuid Uuid::Characteristics::CyclingPowerFeature{0x2a65};
const Uuid Uuid::Characteristics::CyclingPowerMeasurement{0x2a63};
const Uuid Uuid::Characteristics::CyclingPowerVector{0x2a64};
const Uuid Uuid::Characteristics::DSTOffset{0x2a0d};
const Uuid Uuid::Characteristics::DatabaseChangeIncrement{0x2a99};
const Uuid Uuid::Characteristics::DatabaseHash{0x2b2a};
const Uuid Uuid::Characteristics::DateTime{0x2a08};
const Uuid Uuid::Characteristics::DateUTC{0x2aed};
const Uuid Uuid::Characteristics::DateofBirth{0x2a85};
const Uuid Uuid::Characteristics::DateofThresholdAssessment{0x2a86};
const Uuid Uuid::Characteristics::DayDateTime{0x2a0a};
const Uuid Uuid::Characteristics::DayofWeek{0x2a09};
const Uuid Uuid::Characteristics::DescriptorValueChanged{0x2a7d};
const Uuid Uuid::Characteristics::DeviceName{0x2a00};
const Uuid Uuid::Characteristics::DeviceTime{0x2b90};
const Uuid Uuid::Characteristics::DeviceTimeControlPoint{0x2b91};
const Uuid Uuid::Characteristics::DeviceTimeFeature{0x2b8e};
const Uuid Uuid::Characteristics::DeviceTimeParameters{0x2b8f};
const Uuid Uuid::Characteristics::DeviceWearingPosition{0x2b4b};
const Uuid Uuid::Characteristics::DewPoint{0x2a7b};
const Uuid Uuid::Characteristics::ESLAddress{0x2bf6};
const Uuid Uuid::Characteristics::ESLControlPoint{0x2bfe};
const Uuid Uuid::Characteristics::ESLCurrentAbsoluteTime{0x2bf9};
const Uuid Uuid::Characteristics::ESLDisplayInformation{0x2bfa};
const Uuid Uuid::Characteristics::ESLImageInformation{0x2bfb};
const Uuid Uuid::Characteristics::ESLLEDInformation{0x2bfd};
const Uuid Uuid::Characteristics::ESLResponseKeyMaterial{0x2bf8};
const Uuid Uuid::Characteristics::ESLSensorInformation{0x2bfc};
const Uuid Uuid::Characteristics::ElectricCurrent{0x2aee};
const Uuid Uuid::Characteristics::ElectricCurrentRange{0x2aef};
const Uuid Uuid::Characteristics::ElectricCurrentSpecification{0x2af0};
const Uuid Uuid::Characteristics::ElectricCurrentStatistics{0x2af1};
const Uuid Uuid::Characteristics::Elevation{0x2a6c};
const Uuid Uuid::Characteristics::EmailAddress{0x2a87};
const Uuid Uuid::Characteristics::EmergencyID{0x2b2d};
const Uuid Uuid::Characteristics::EmergencyText{0x2b2e};
const Uuid Uuid::Characteristics::EncryptedDataKeyMaterial{0x2b88};
const Uuid Uuid::Characteristics::Energy{0x2af2};
const Uuid Uuid::Characteristics::Energy32{0x2ba8};
const Uuid Uuid::Characteristics::EnergyinaPeriodofDay{0x2af3};
const Uuid Uuid::Characteristics::EnhancedBloodPressureMeasurement{0x2b34};
const Uuid Uuid::Characteristics::EnhancedIntermediateCuffPressure{0x2b35};
const Uuid Uuid::Characteristics::EstimatedServiceDate{0x2bef};
const Uuid Uuid::Characteristics::EventStatistics{0x2af4};
const Uuid Uuid::Characteristics::ExactTime256{0x2a0c};
const Uuid Uuid::Characteristics::FatBurnHeartRateLowerLimit{0x2a88};
const Uuid Uuid::Characteristics::FatBurnHeartRateUpperLimit{0x2a89};
const Uuid Uuid::Characteristics::FirmwareRevisionString{0x2a26};
const Uuid Uuid::Characteristics::FirstName{0x2a8a};
const Uuid Uuid::Characteristics::FirstUseDate{0x2c0e};
const Uuid Uuid::Characteristics::FitnessMachineControlPoint{0x2ad9};
const Uuid Uuid::Characteristics::FitnessMachineFeature{0x2acc};
const Uuid Uuid::Characteristics::FitnessMachineStatus{0x2ada};
const Uuid Uuid::Characteristics::FiveZoneHeartRateLimits{0x2a8b};
const Uuid Uuid::Characteristics::FixedString16{0x2af5};
const Uuid Uuid::Characteristics::FixedString24{0x2af6};
const Uuid Uuid::Characteristics::FixedString36{0x2af7};
const Uuid Uuid::Characteristics::FixedString64{0x2bde};
const Uuid Uuid::Characteristics::FixedString8{0x2af8};
const Uuid Uuid::Characteristics::FloorNumber{0x2ab2};
const Uuid Uuid::Characteristics::Force{0x2c07};
const Uuid Uuid::Characteristics::FourZoneHeartRateLimits{0x2b4c};
const Uuid Uuid::Characteristics::GHSControlPoint{0x2bf4};
const Uuid Uuid::Characteristics::GMAPRole{0x2c00};
const Uuid Uuid::Characteristics::GainSettingsAttribute{0x2b78};
const Uuid Uuid::Characteristics::Gender{0x2a8c};
const Uuid Uuid::Characteristics::GeneralActivityInstantaneousData{0x2b3c};
const Uuid Uuid::Characteristics::GeneralActivitySummaryData{0x2b3d};
const Uuid Uuid::Characteristics::GenericLevel{0x2af9};
const Uuid Uuid::Characteristics::GlobalTradeItemNumber{0x2afa};
const Uuid Uuid::Characteristics::GlucoseFeature{0x2a51};
const Uuid Uuid::Characteristics::GlucoseMeasurement{0x2a18};
const Uuid Uuid::Characteristics::GlucoseMeasurementContext{0x2a34};
const Uuid Uuid::Characteristics::GustFactor{0x2a74};
const Uuid Uuid::Characteristics::HIDControlPoint{0x2a4c};
const Uuid Uuid::Characteristics::HIDInformation{0x2a4a};
const Uuid Uuid::Characteristics::HTTPControlPoint{0x2aba};
const Uuid Uuid::Characteristics::HTTPEntityBody{0x2ab9};
const Uuid Uuid::Characteristics::HTTPHeaders{0x2ab7};
const Uuid Uuid::Characteristics::HTTPStatusCode{0x2ab8};
const Uuid Uuid::Characteristics::HTTPSSecurity{0x2abb};
const Uuid Uuid::Characteristics::Handedness{0x2b4a};
const Uuid Uuid::Characteristics::HardwareRevisionString{0x2a27};
const Uuid Uuid::Characteristics::HealthSensorFeatures{0x2bf3};
const Uuid Uuid::Characteristics::HearingAidFeatures{0x2bda};
const Uuid Uuid::Characteristics::HearingAidPresetControlPoint{0x2bdb};
const Uuid Uuid::Characteristics::HeartRateControlPoint{0x2a39};
const Uuid Uuid::Characteristics::HeartRateMax{0x2a8d};
const Uuid Uuid::Characteristics::HeartRateMeasurement{0x2a37};
const Uuid Uuid::Characteristics::HeatIndex{0x2a7a};
const Uuid Uuid::Characteristics::Height{0x2a8e};
const Uuid Uuid::Characteristics::HighIntensityExerciseThreshold{0x2b4d};
const Uuid Uuid::Characteristics::HighResolutionHeight{0x2b47};
const Uuid Uuid::Characteristics::HighTemperature{0x2bdf};
const Uuid Uuid::Characteristics::HighVoltage{0x2be0};
const Uuid Uuid::Characteristics::HipCircumference{0x2a8f};
const Uuid Uuid::Characteristics::Humidity{0x2a6f};
const Uuid Uuid::Characteristics::IDDAnnunciationStatus{0x2b22};
const Uuid Uuid::Characteristics::IDDCommandControlPoint{0x2b25};
const Uuid Uuid::Characteristics::IDDCommandData{0x2b26};
const Uuid Uuid::Characteristics::IDDFeatures{0x2b23};
const Uuid Uuid::Characteristics::IDDHistoryData{0x2b28};
const Uuid Uuid::Characteristics::IDDRecordAccessControlPoint{0x2b27};
const Uuid Uuid::Characteristics::IDDStatus{0x2b21};
const Uuid Uuid::Characteristics::IDDStatusChanged{0x2b20};
const Uuid Uuid::Characteristics::IDDStatusReaderControlPoint{0x2b24};
const Uuid Uuid::Characteristics::IEEE11073_20601RegulatoryCertificationDataList{0x2a2a};
const Uuid Uuid::Characteristics::IMDControl{0x2c12};
const Uuid Uuid::Characteristics::IMDHistoricalData{0x2c13};
const Uuid Uuid::Characteristics::IMDStatus{0x2c0c};
const Uuid Uuid::Characteristics::IMDSDescriptorValueChanged{0x2c0d};
const Uuid Uuid::Characteristics::Illuminance{0x2afb};
const Uuid Uuid::Characteristics::IncomingCall{0x2bc1};
const Uuid Uuid::Characteristics::IncomingCallTargetBearerURI{0x2bbc};
const Uuid Uuid::Characteristics::IndoorBikeData{0x2ad2};
const Uuid Uuid::Characteristics::IndoorPositioningConfiguration{0x2aad};
const Uuid Uuid::Characteristics::IntermediateCuffPressure{0x2a36};
const Uuid Uuid::Characteristics::IntermediateTemperature{0x2a1e};
const Uuid Uuid::Characteristics::Irradiance{0x2a77};
const Uuid Uuid::Characteristics::LEGATTSecurityLevels{0x2bf5};
const Uuid Uuid::Characteristics::LNControlPoint{0x2a6b};
const Uuid Uuid::Characteristics::LNFeature{0x2a6a};
const Uuid Uuid::Characteristics::Language{0x2aa2};
const Uuid Uuid::Characteristics::LastName{0x2a90};
const Uuid Uuid::Characteristics::Latitude{0x2aae};
const Uuid Uuid::Characteristics::Length{0x2c0a};
const Uuid Uuid::Characteristics::LifeCycleData{0x2c0f};
const Uuid Uuid::Characteristics::LightDistribution{0x2be1};
const Uuid Uuid::Characteristics::LightOutput{0x2be2};
const Uuid Uuid::Characteristics::LightSourceType{0x2be3};
const Uuid Uuid::Characteristics::LinearPosition{0x2c08};
const Uuid Uuid::Characteristics::LiveHealthObservations{0x2b8b};
const Uuid Uuid::Characteristics::LocalEastCoordinate{0x2ab1};
const Uuid Uuid::Characteristics::LocalNorthCoordinate{0x2ab0};
const Uuid Uuid::Characteristics::LocalTimeInformation{0x2a0f};
const Uuid Uuid::Characteristics::LocationName{0x2ab5};
const Uuid Uuid::Characteristics::LocationandSpeed{0x2a67};
const Uuid Uuid::Characteristics::Longitude{0x2aaf};
const Uuid Uuid::Characteristics::LuminousEfficacy{0x2afc};
const Uuid Uuid::Characteristics::LuminousEnergy{0x2afd};
const Uuid Uuid::Characteristics::LuminousExposure{0x2afe};
const Uuid Uuid::Characteristics::LuminousFlux{0x2aff};
const Uuid Uuid::Characteristics::LuminousFluxRange{0x2b00};
const Uuid Uuid::Characteristics::LuminousIntensity{0x2b01};
const Uuid Uuid::Characteristics::MagneticDeclination{0x2a2c};
const Uuid Uuid::Characteristics::MagneticFluxDensity_2D{0x2aa0};
const Uuid Uuid::Characteristics::MagneticFluxDensity_3D{0x2aa1};
const Uuid Uuid::Characteristics::ManufacturerNameString{0x2a29};
const Uuid Uuid::Characteristics::MassFlow{0x2b02};
const Uuid Uuid::Characteristics::MaximumRecommendedHeartRate{0x2a91};
const Uuid Uuid::Characteristics::MeasurementInterval{0x2a21};
const Uuid Uuid::Characteristics::MediaControlPoint{0x2ba4};
const Uuid Uuid::Characteristics::MediaControlPointOpcodesSupported{0x2ba5};
const Uuid Uuid::Characteristics::MediaPlayerIconObjectID{0x2b94};
const Uuid Uuid::Characteristics::MediaPlayerIconURL{0x2b95};
const Uuid Uuid::Characteristics::MediaPlayerName{0x2b93};
const Uuid Uuid::Characteristics::MediaState{0x2ba3};
const Uuid Uuid::Characteristics::MeshProvisioningDataIn{0x2adb};
const Uuid Uuid::Characteristics::MeshProvisioningDataOut{0x2adc};
const Uuid Uuid::Characteristics::MeshProxyDataIn{0x2add};
const Uuid Uuid::Characteristics::MeshProxyDataOut{0x2ade};
const Uuid Uuid::Characteristics::MethaneConcentration{0x2bd1};
const Uuid Uuid::Characteristics::MiddleName{0x2b48};
const Uuid Uuid::Characteristics::ModelNumberString{0x2a24};
const Uuid Uuid::Characteristics::Mute{0x2bc3};
const Uuid Uuid::Characteristics::Navigation{0x2a68};
const Uuid Uuid::Characteristics::NewAlert{0x2a46};
const Uuid Uuid::Characteristics::NextTrackObjectID{0x2b9e};
const Uuid Uuid::Characteristics::NitrogenDioxideConcentration{0x2bd2};
const Uuid Uuid::Characteristics::Noise{0x2be4};
const Uuid Uuid::Characteristics::Non_MethaneVolatileOrganicCompoundsConcentration{0x2bd3};
const Uuid Uuid::Characteristics::OTSFeature{0x2abd};
const Uuid Uuid::Characteristics::ObjectActionControlPoint{0x2ac5};
const Uuid Uuid::Characteristics::ObjectChanged{0x2ac8};
const Uuid Uuid::Characteristics::ObjectFirst_Created{0x2ac1};
const Uuid Uuid::Characteristics::ObjectID{0x2ac3};
const Uuid Uuid::Characteristics::ObjectLast_Modified{0x2ac2};
const Uuid Uuid::Characteristics::ObjectListControlPoint{0x2ac6};
const Uuid Uuid::Characteristics::ObjectListFilter{0x2ac7};
const Uuid Uuid::Characteristics::ObjectName{0x2abe};
const Uuid Uuid::Characteristics::ObjectProperties{0x2ac4};
const Uuid Uuid::Characteristics::ObjectSize{0x2ac0};
const Uuid Uuid::Characteristics::ObjectType{0x2abf};
const Uuid Uuid::Characteristics::ObservationScheduleChanged{0x2bf1};
const Uuid Uuid::Characteristics::On_demandRangingData{0x2c16};
const Uuid Uuid::Characteristics::OzoneConcentration{0x2bd4};
const Uuid Uuid::Characteristics::PLXContinuousMeasurement{0x2a5f};
const Uuid Uuid::Characteristics::PLXFeatures{0x2a60};
const Uuid Uuid::Characteristics::PLXSpot_CheckMeasurement{0x2a5e};
const Uuid Uuid::Characteristics::ParentGroupObjectID{0x2b9f};
const Uuid Uuid::Characteristics::ParticulateMatter_PM1Concentration{0x2bd5};
const Uuid Uuid::Characteristics::ParticulateMatter_PM10Concentration{0x2bd7};
const Uuid Uuid::Characteristics::ParticulateMatter_PM2_5Concentration{0x2bd6};
const Uuid Uuid::Characteristics::PerceivedLightness{0x2b03};
const Uuid Uuid::Characteristics::Percentage8{0x2b04};
const Uuid Uuid::Characteristics::Percentage8Steps{0x2c05};
const Uuid Uuid::Characteristics::PeripheralPreferredConnectionParameters{0x2a04};
const Uuid Uuid::Characteristics::PeripheralPrivacyFlag{0x2a02};
const Uuid Uuid::Characteristics::PhysicalActivityCurrentSession{0x2b44};
const Uuid Uuid::Characteristics::PhysicalActivityMonitorControlPoint{0x2b43};
const Uuid Uuid::Characteristics::PhysicalActivityMonitorFeatures{0x2b3b};
const Uuid Uuid::Characteristics::PhysicalActivitySessionDescriptor{0x2b45};
const Uuid Uuid::Characteristics::PlaybackSpeed{0x2b9a};
const Uuid Uuid::Characteristics::PlayingOrder{0x2ba1};
const Uuid Uuid::Characteristics::PlayingOrdersSupported{0x2ba2};
const Uuid Uuid::Characteristics::PnPID{0x2a50};
const Uuid Uuid::Characteristics::PollenConcentration{0x2a75};
const Uuid Uuid::Characteristics::PositionQuality{0x2a69};
const Uuid Uuid::Characteristics::Power{0x2b05};
const Uuid Uuid::Characteristics::PowerSpecification{0x2b06};
const Uuid Uuid::Characteristics::PreferredUnits{0x2b46};
const Uuid Uuid::Characteristics::Pressure{0x2a6d};
const Uuid Uuid::Characteristics::ProtocolMode{0x2a4e};
const Uuid Uuid::Characteristics::RASControlPoint{0x2c17};
const Uuid Uuid::Characteristics::RASFeatures{0x2c14};
const Uuid Uuid::Characteristics::RCFeature{0x2b1d};
const Uuid Uuid::Characteristics::RCSettings{0x2b1e};
const Uuid Uuid::Characteristics::RSCFeature{0x2a54};
const Uuid Uuid::Characteristics::RSCMeasurement{0x2a53};
const Uuid Uuid::Characteristics::Rainfall{0x2a78};
const Uuid Uuid::Characteristics::RangingDataOverwritten{0x2c19};
const Uuid Uuid::Characteristics::RangingDataReady{0x2c18};
const Uuid Uuid::Characteristics::Real_timeRangingData{0x2c15};
const Uuid Uuid::Characteristics::ReconnectionAddress{0x2a03};
const Uuid Uuid::Characteristics::ReconnectionConfigurationControlPoint{0x2b1f};
const Uuid Uuid::Characteristics::RecordAccessControlPoint{0x2a52};
const Uuid Uuid::Characteristics::ReferenceTimeInformation{0x2a14};
const Uuid Uuid::Characteristics::RegisteredUser{0x2b37};
const Uuid Uuid::Characteristics::RelativeRuntimeinaCorrelatedColorTemperatureRange{0x2be5};
const Uuid Uuid::Characteristics::RelativeRuntimeinaCurrentRange{0x2b07};
const Uuid Uuid::Characteristics::RelativeRuntimeinaGenericLevelRange{0x2b08};
const Uuid Uuid::Characteristics::RelativeValueinaPeriodofDay{0x2b0b};
const Uuid Uuid::Characteristics::RelativeValueinaTemperatureRange{0x2b0c};
const Uuid Uuid::Characteristics::RelativeValueinaVoltageRange{0x2b09};
const Uuid Uuid::Characteristics::RelativeValueinanIlluminanceRange{0x2b0a};
const Uuid Uuid::Characteristics::Report{0x2a4d};
const Uuid Uuid::Characteristics::ReportMap{0x2a4b};
const Uuid Uuid::Characteristics::ResolvablePrivateAddressOnly{0x2ac9};
const Uuid Uuid::Characteristics::RestingHeartRate{0x2a92};
const Uuid Uuid::Characteristics::RingerControlPoint{0x2a40};
const Uuid Uuid::Characteristics::RingerSetting{0x2a41};
const Uuid Uuid::Characteristics::RotationalSpeed{0x2c09};
const Uuid Uuid::Characteristics::RowerData{0x2ad1};
const Uuid Uuid::Characteristics::SCControlPoint{0x2a55};
const Uuid Uuid::Characteristics::ScanIntervalWindow{0x2a4f};
const Uuid Uuid::Characteristics::ScanRefresh{0x2a31};
const Uuid Uuid::Characteristics::SearchControlPoint{0x2ba7};
const Uuid Uuid::Characteristics::SearchResultsObjectID{0x2ba6};
const Uuid Uuid::Characteristics::SedentaryIntervalNotification{0x2b4f};
const Uuid Uuid::Characteristics::SeekingSpeed{0x2b9b};
const Uuid Uuid::Characteristics::SensorLocation{0x2a5d};
const Uuid Uuid::Characteristics::SerialNumberString{0x2a25};
const Uuid Uuid::Characteristics::ServerSupportedFeatures{0x2b3a};
const Uuid Uuid::Characteristics::ServiceChanged{0x2a05};
const Uuid Uuid::Characteristics::ServiceCycleData{0x2c11};
const Uuid Uuid::Characteristics::SetIdentityResolvingKey{0x2b84};
const Uuid Uuid::Characteristics::SetMemberLock{0x2b86};
const Uuid Uuid::Characteristics::SetMemberRank{0x2b87};
const Uuid Uuid::Characteristics::SinkASE{0x2bc4};
const Uuid Uuid::Characteristics::SinkAudioLocations{0x2bca};
const Uuid Uuid::Characteristics::SinkPAC{0x2bc9};
const Uuid Uuid::Characteristics::SleepActivityInstantaneousData{0x2b41};
const Uuid Uuid::Characteristics::SleepActivitySummaryData{0x2b42};
const Uuid Uuid::Characteristics::SoftwareRevisionString{0x2a28};
const Uuid Uuid::Characteristics::SourceASE{0x2bc5};
const Uuid Uuid::Characteristics::SourceAudioLocations{0x2bcc};
const Uuid Uuid::Characteristics::SourcePAC{0x2bcb};
const Uuid Uuid::Characteristics::SportTypeforAerobicandAnaerobicThresholds{0x2a93};
const Uuid Uuid::Characteristics::StairClimberData{0x2ad0};
const Uuid Uuid::Characteristics::StatusFlags{0x2bbb};
const Uuid Uuid::Characteristics::StepClimberData{0x2acf};
const Uuid Uuid::Characteristics::StepCounterActivitySummaryData{0x2b40};
const Uuid Uuid::Characteristics::StoredHealthObservations{0x2bdd};
const Uuid Uuid::Characteristics::StrideLength{0x2b49};
const Uuid Uuid::Characteristics::SulfurDioxideConcentration{0x2bd8};
const Uuid Uuid::Characteristics::SulfurHexafluorideConcentration{0x2bd9};
const Uuid Uuid::Characteristics::SupportedAudioContexts{0x2bce};
const Uuid Uuid::Characteristics::SupportedHeartRateRange{0x2ad7};
const Uuid Uuid::Characteristics::SupportedInclinationRange{0x2ad5};
const Uuid Uuid::Characteristics::SupportedNewAlertCategory{0x2a47};
const Uuid Uuid::Characteristics::SupportedPowerRange{0x2ad8};
const Uuid Uuid::Characteristics::SupportedResistanceLevelRange{0x2ad6};
const Uuid Uuid::Characteristics::SupportedSpeedRange{0x2ad4};
const Uuid Uuid::Characteristics::SupportedUnreadAlertCategory{0x2a48};
const Uuid Uuid::Characteristics::SystemID{0x2a23};
const Uuid Uuid::Characteristics::TDSControlPoint{0x2abc};
const Uuid Uuid::Characteristics::TMAPRole{0x2b51};
const Uuid Uuid::Characteristics::Temperature{0x2a6e};
const Uuid Uuid::Characteristics::Temperature8{0x2b0d};
const Uuid Uuid::Characteristics::Temperature8Statistics{0x2b0f};
const Uuid Uuid::Characteristics::Temperature8inaPeriodofDay{0x2b0e};
const Uuid Uuid::Characteristics::TemperatureMeasurement{0x2a1c};
const Uuid Uuid::Characteristics::TemperatureRange{0x2b10};
const Uuid Uuid::Characteristics::TemperatureStatistics{0x2b11};
const Uuid Uuid::Characteristics::TemperatureType{0x2a1d};
const Uuid Uuid::Characteristics::TerminationReason{0x2bc0};
const Uuid Uuid::Characteristics::ThreeZoneHeartRateLimits{0x2a94};
const Uuid Uuid::Characteristics::TimeAccuracy{0x2a12};
const Uuid Uuid::Characteristics::TimeChangeLogData{0x2b92};
const Uuid Uuid::Characteristics::TimeDecihour8{0x2b12};
const Uuid Uuid::Characteristics::TimeExponential8{0x2b13};
const Uuid Uuid::Characteristics::TimeHour24{0x2b14};
const Uuid Uuid::Characteristics::TimeMillisecond24{0x2b15};
const Uuid Uuid::Characteristics::TimeSecond16{0x2b16};
const Uuid Uuid::Characteristics::TimeSecond32{0x2be6};
const Uuid Uuid::Characteristics::TimeSecond8{0x2b17};
const Uuid Uuid::Characteristics::TimeSource{0x2a13};
const Uuid Uuid::Characteristics::TimeUpdateControlPoint{0x2a16};
const Uuid Uuid::Characteristics::TimeUpdateState{0x2a17};
const Uuid Uuid::Characteristics::TimeZone{0x2a0e};
const Uuid Uuid::Characteristics::TimewithDST{0x2a11};
const Uuid Uuid::Characteristics::Torque{0x2c0b};
const Uuid Uuid::Characteristics::TrackChanged{0x2b96};
const Uuid Uuid::Characteristics::TrackDuration{0x2b98};
const Uuid Uuid::Characteristics::TrackPosition{0x2b99};
const Uuid Uuid::Characteristics::TrackTitle{0x2b97};
const Uuid Uuid::Characteristics::TrainingStatus{0x2ad3};
const Uuid Uuid::Characteristics::TreadmillData{0x2acd};
const Uuid Uuid::Characteristics::TrueWindDirection{0x2a71};
const Uuid Uuid::Characteristics::TrueWindSpeed{0x2a70};
const Uuid Uuid::Characteristics::TwoZoneHeartRateLimits{0x2a95};
const Uuid Uuid::Characteristics::TxPowerLevel{0x2a07};
const Uuid Uuid::Characteristics::UDIforMedicalDevices{0x2bff};
const Uuid Uuid::Characteristics::UGGFeatures{0x2c01};
const Uuid Uuid::Characteristics::UGTFeatures{0x2c02};
const Uuid Uuid::Characteristics::URI{0x2ab6};
const Uuid Uuid::Characteristics::UVIndex{0x2a76};
const Uuid Uuid::Characteristics::Uncertainty{0x2ab4};
const Uuid Uuid::Characteristics::UnreadAlertStatus{0x2a45};
const Uuid Uuid::Characteristics::UserControlPoint{0x2a9f};
const Uuid Uuid::Characteristics::UserIndex{0x2a9a};
const Uuid Uuid::Characteristics::VO2Max{0x2a96};
const Uuid Uuid::Characteristics::VOCConcentration{0x2be7};
const Uuid Uuid::Characteristics::Voltage{0x2b18};
const Uuid Uuid::Characteristics::VoltageFrequency{0x2be8};
const Uuid Uuid::Characteristics::VoltageSpecification{0x2b19};
const Uuid Uuid::Characteristics::VoltageStatistics{0x2b1a};
const Uuid Uuid::Characteristics::VolumeControlPoint{0x2b7e};
const Uuid Uuid::Characteristics::VolumeFlags{0x2b7f};
const Uuid Uuid::Characteristics::VolumeFlow{0x2b1b};
const Uuid Uuid::Characteristics::VolumeOffsetControlPoint{0x2b82};
const Uuid Uuid::Characteristics::VolumeOffsetState{0x2b80};
const Uuid Uuid::Characteristics::VolumeState{0x2b7d};
const Uuid Uuid::Characteristics::WaistCircumference{0x2a97};
const Uuid Uuid::Characteristics::Weight{0x2a98};
const Uuid Uuid::Characteristics::WeightMeasurement{0x2a9d};
const Uuid Uuid::Characteristics::WeightScaleFeature{0x2a9e};
const Uuid Uuid::Characteristics::WindChill{0x2a79};
const Uuid Uuid::Characteristics::WorkCycleData{0x2c10};
const Uuid Uuid::ServiceClasses::ServiceDiscoveryServerServiceClassID{0x1000};
const Uuid Uuid::ServiceClasses::BrowseGroupDescriptorServiceClassID{0x1001};
const Uuid Uuid::ServiceClasses::SerialPort{0x1101};
const Uuid Uuid::ServiceClasses::LANAccessUsingPPP{0x1102};
const Uuid Uuid::ServiceClasses::DialUpNetworking{0x1103};
const Uuid Uuid::ServiceClasses::IrMCSync{0x1104};
const Uuid Uuid::ServiceClasses::OBEXObjectPush{0x1105};
const Uuid Uuid::ServiceClasses::OBEXFileTransfer{0x1106};
const Uuid Uuid::ServiceClasses::IrMCSyncCommand{0x1107};
const Uuid Uuid::ServiceClasses::Headset{0x1108};
const Uuid Uuid::ServiceClasses::CordlessTelephony{0x1109};
const Uuid Uuid::ServiceClasses::AudioSource{0x110a};
const Uuid Uuid::ServiceClasses::AudioSink{0x110b};
const Uuid Uuid::ServiceClasses::AVRemoteControlTarget{0x110c};
const Uuid Uuid::ServiceClasses::AdvancedAudioDistribution{0x110d};
const Uuid Uuid::ServiceClasses::AVRemoteControl{0x110e};
const Uuid Uuid::ServiceClasses::AVRemoteControlController{0x110f};
const Uuid Uuid::ServiceClasses::Intercom{0x1110};
const Uuid Uuid::ServiceClasses::Fax{0x1111};
const Uuid Uuid::ServiceClasses::HeadsetAudioGateway{0x1112};
const Uuid Uuid::ServiceClasses::WAP{0x1113};
const Uuid Uuid::ServiceClasses::WAP_CLIENT{0x1114};
const Uuid Uuid::ServiceClasses::PANU{0x1115};
const Uuid Uuid::ServiceClasses::NAP{0x1116};
const Uuid Uuid::ServiceClasses::GN{0x1117};
const Uuid Uuid::ServiceClasses::DirectPrinting{0x1118};
const Uuid Uuid::ServiceClasses::ReferencePrinting{0x1119};
const Uuid Uuid::ServiceClasses::Imaging{0x111a};
const Uuid Uuid::ServiceClasses::ImagingResponder{0x111b};
const Uuid Uuid::ServiceClasses::ImagingAutomaticArchive{0x111c};
const Uuid Uuid::ServiceClasses::ImagingReferencedObjects{0x111d};
const Uuid Uuid::ServiceClasses::HandsFree{0x111e};
const Uuid Uuid::ServiceClasses::AGHandsFree{0x111f};
const Uuid Uuid::ServiceClasses::DirectPrintingReferencedObjectsService{0x1120};
const Uuid Uuid::ServiceClasses::ReflectedUI{0x1121};
const Uuid Uuid::ServiceClasses::BasicPrinting{0x1122};
const Uuid Uuid::ServiceClasses::PrintingStatus{0x1123};
const Uuid Uuid::ServiceClasses::HID{0x1124};
const Uuid Uuid::ServiceClasses::HardcopyCableReplacement{0x1125};
const Uuid Uuid::ServiceClasses::HCR_Print{0x1126};
const Uuid Uuid::ServiceClasses::HCR_Scan{0x1127};
const Uuid Uuid::ServiceClasses::Common_ISDN_Access{0x1128};
const Uuid Uuid::ServiceClasses::SIMAccess{0x112d};
const Uuid Uuid::ServiceClasses::PhonebookAccessClient{0x112e};
const Uuid Uuid::ServiceClasses::PhonebookAccessServer{0x112f};
const Uuid Uuid::ServiceClasses::PhonebookAccessProfile{0x1130};
const Uuid Uuid::ServiceClasses::HeadsetHS{0x1131};
const Uuid Uuid::ServiceClasses::MessageAccessServer{0x1132};
const Uuid Uuid::ServiceClasses::MessageNotificationServer{0x1133};
const Uuid Uuid::ServiceClasses::MessageAccessProfile{0x1134};
const Uuid Uuid::ServiceClasses::GNSS{0x1135};
const Uuid Uuid::ServiceClasses::GNSS_Server{0x1136};
const Uuid Uuid::ServiceClasses::Display3D{0x1137};
const Uuid Uuid::ServiceClasses::Glasses3D{0x1138};
const Uuid Uuid::ServiceClasses::SynchProfile3D{0x1139};
const Uuid Uuid::ServiceClasses::MultiProfileSpecification{0x113a};
const Uuid Uuid::ServiceClasses::MPS{0x113b};
const Uuid Uuid::ServiceClasses::CTNAccessService{0x113c};
const Uuid Uuid::ServiceClasses::CTNNotificationService{0x113d};
const Uuid Uuid::ServiceClasses::CalendarTasksandNotesProfile{0x113e};
const Uuid Uuid::ServiceClasses::PnPInformation{0x1200};
const Uuid Uuid::ServiceClasses::GenericNetworking{0x1201};
const Uuid Uuid::ServiceClasses::GenericFileTransfer{0x1202};
const Uuid Uuid::ServiceClasses::GenericAudio{0x1203};
const Uuid Uuid::ServiceClasses::GenericTelephony{0x1204};
const Uuid Uuid::ServiceClasses::UPNP_Service{0x1205};
const Uuid Uuid::ServiceClasses::UPNP_IP_Service{0x1206};
const Uuid Uuid::ServiceClasses::ESDP_UPNP_IP_PAN{0x1300};
const Uuid Uuid::ServiceClasses::ESDP_UPNP_IP_LAP{0x1301};
const Uuid Uuid::ServiceClasses::ESDP_UPNP_L2CAP{0x1302};
const Uuid Uuid::ServiceClasses::VideoSource{0x1303};
const Uuid Uuid::ServiceClasses::VideoSink{0x1304};
const Uuid Uuid::ServiceClasses::VideoDistribution{0x1305};
const Uuid Uuid::ServiceClasses::HDP{0x1400};
const Uuid Uuid::ServiceClasses::HDPSource{0x1401};
const Uuid Uuid::ServiceClasses::HDPSink{0x1402};

}  // namespace bluetooth
