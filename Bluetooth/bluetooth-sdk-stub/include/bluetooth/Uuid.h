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


/**
 * @file Uuid.h
 * @brief Defines the Uuid class for handling Bluetooth UUIDs.
 *
 * This class provides functionality to represent and compare Bluetooth UUIDs,
 * supporting both 16-bit and string-based initialization.
 */

#pragma once

#include <cstdint>
#include <cstring>
#include <string>

namespace bluetooth {

/**
 * @class Uuid
 * @brief Represents a Bluetooth UUID and provides comparison and formatting utilities.
 *
 * The Uuid class supports initialization from a 16-bit short UUID or a full string UUID.
 * It provides methods for converting to string format and comparison operators.
 */
class Uuid {
 public:
  /**
   * @brief Constructs a Uuid from a 16-bit identifier.
   * @param id The 16-bit UUID value.
   */
  explicit Uuid(uint16_t id);

  /**
   * @brief Constructs a Uuid from a string representation.
   * @param id The UUID as a string (e.g., "0000180D-0000-1000-8000-00805F9B34FB").
   */
  explicit Uuid(const std::string& id);

  /**
   * @brief Converts the UUID to a string representation.
   * @param shortForm If true, returns the short form (16-bit) if possible; otherwise, full UUID.
   * @return The UUID as a string.
   */
  std::string toString(bool shortForm = true) const;

  /**
   * @brief Less-than operator for comparing UUIDs.
   * @param rhs The UUID to compare against.
   * @return True if this UUID is less than rhs.
   */
  bool operator<(const Uuid& rhs) const { return this->compare(rhs) < 0; }

  /**
   * @brief Greater-than operator for comparing UUIDs.
   * @param rhs The UUID to compare against.
   * @return True if this UUID is greater than rhs.
   */
  bool operator>(const Uuid& rhs) const { return this->compare(rhs) > 0; }

  /**
   * @brief Equality operator for comparing UUIDs.
   * @param rhs The UUID to compare against.
   * @return True if both UUIDs are equal.
   */
  bool operator==(const Uuid& rhs) const { return this->compare(rhs) == 0; }

  /**
   * @brief Compares this UUID with another.
   * @param rhs The UUID to compare against.
   * @return An integer less than, equal to, or greater than zero if this UUID is
   *         less than, equal to, or greater than rhs.
   */
  inline int compare(const Uuid& rhs) const { return std::memcmp(this->m_uuid, rhs.m_uuid, 16); }


 public:
  // https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/Assigned_Numbers/out/en/Assigned_Numbers.pdf?v=1738197119246
  // yaml files
  // https://bitbucket.org/bluetooth-SIG/public/src/main/assigned_numbers/
  struct Services {
    static const Uuid AlertNotificationService;
    static const Uuid AudioInputControlService;
    static const Uuid AudioStreamControlService;
    static const Uuid AuthorizationControlService;
    static const Uuid AutomationIOService;
    static const Uuid BasicAudioAnnouncementService;
    static const Uuid BatteryService;
    static const Uuid BinarySensorService;
    static const Uuid BloodPressureService;
    static const Uuid BodyCompositionService;
    static const Uuid BondManagementService;
    static const Uuid BroadcastAudioAnnouncementService;
    static const Uuid BroadcastAudioScanService;
    static const Uuid CommonAudioService;
    static const Uuid ConstantToneExtensionService;
    static const Uuid ContinuousGlucoseMonitoringService;
    static const Uuid CoordinatedSetIdentificationService;
    static const Uuid CurrentTimeService;
    static const Uuid CyclingPowerService;
    static const Uuid CyclingSpeedandCadenceService;
    static const Uuid DeviceInformationService;
    static const Uuid DeviceTimeService;
    static const Uuid ElapsedTimeService;
    static const Uuid ElectronicShelfLabelService;
    static const Uuid EmergencyConfigurationService;
    static const Uuid EnvironmentalSensingService;
    static const Uuid FitnessMachineService;
    static const Uuid GAPService;
    static const Uuid GATTService;
    static const Uuid GamingAudioService;
    static const Uuid GenericHealthSensorService;
    static const Uuid GenericMediaControlService;
    static const Uuid GenericTelephoneBearerService;
    static const Uuid GlucoseService;
    static const Uuid HTTPProxyService;
    static const Uuid HealthThermometerService;
    static const Uuid HearingAccessService;
    static const Uuid HeartRateService;
    static const Uuid HumanInterfaceDeviceService;
    static const Uuid ImmediateAlertService;
    static const Uuid IndoorPositioningService;
    static const Uuid IndustrialMeasurementDeviceService;
    static const Uuid InsulinDeliveryService;
    static const Uuid InternetProtocolSupportService;
    static const Uuid LinkLossService;
    static const Uuid LocationandNavigationService;
    static const Uuid MediaControlService;
    static const Uuid MeshProvisioningService;
    static const Uuid MeshProxyService;
    static const Uuid MeshProxySolicitationService;
    static const Uuid MicrophoneControlService;
    static const Uuid NextDSTChangeService;
    static const Uuid ObjectTransferService;
    static const Uuid PhoneAlertStatusService;
    static const Uuid PhysicalActivityMonitorService;
    static const Uuid PublicBroadcastAnnouncementService;
    static const Uuid PublishedAudioCapabilitiesService;
    static const Uuid PulseOximeterService;
    static const Uuid RangingService;
    static const Uuid ReconnectionConfigurationService;
    static const Uuid ReferenceTimeUpdateService;
    static const Uuid RunningSpeedandCadenceService;
    static const Uuid ScanParametersService;
    static const Uuid TelephoneBearerService;
    static const Uuid TelephonyandMediaAudioService;
    static const Uuid TransportDiscoveryService;
    static const Uuid TxPowerService;
    static const Uuid UserDataService;
    static const Uuid VolumeControlService;
    static const Uuid VolumeOffsetControlService;
    static const Uuid WeightScaleService;
  };

  struct Characteristics {
    static const Uuid ACSControlPoint;
    static const Uuid ACSDataIn;
    static const Uuid ACSDataOutIndicate;
    static const Uuid ACSDataOutNotify;
    static const Uuid ACSStatus;
    static const Uuid APSyncKeyMaterial;
    static const Uuid ASEControlPoint;
    static const Uuid Acceleration;
    static const Uuid ActivePresetIndex;
    static const Uuid ActivityGoal;
    static const Uuid AdvertisingConstantToneExtensionInterval;
    static const Uuid AdvertisingConstantToneExtensionMinimumLength;
    static const Uuid AdvertisingConstantToneExtensionMinimumTransmitCount;
    static const Uuid AdvertisingConstantToneExtensionPHY;
    static const Uuid AdvertisingConstantToneExtensionTransmitDuration;
    static const Uuid AerobicHeartRateLowerLimit;
    static const Uuid AerobicHeartRateUpperLimit;
    static const Uuid AerobicThreshold;
    static const Uuid Age;
    static const Uuid Aggregate;
    static const Uuid AlertCategoryID;
    static const Uuid AlertCategoryIDBitMask;
    static const Uuid AlertLevel;
    static const Uuid AlertNotificationControlPoint;
    static const Uuid AlertStatus;
    static const Uuid Altitude;
    static const Uuid AmmoniaConcentration;
    static const Uuid AnaerobicHeartRateLowerLimit;
    static const Uuid AnaerobicHeartRateUpperLimit;
    static const Uuid AnaerobicThreshold;
    static const Uuid ApparentEnergy32;
    static const Uuid ApparentPower;
    static const Uuid ApparentWindDirection;
    static const Uuid ApparentWindSpeed;
    static const Uuid Appearance;
    static const Uuid AudioInputControlPoint;
    static const Uuid AudioInputDescription;
    static const Uuid AudioInputState;
    static const Uuid AudioInputStatus;
    static const Uuid AudioInputType;
    static const Uuid AudioLocation;
    static const Uuid AudioOutputDescription;
    static const Uuid AvailableAudioContexts;
    static const Uuid AverageCurrent;
    static const Uuid AverageVoltage;
    static const Uuid BGRFeatures;
    static const Uuid BGSFeatures;
    static const Uuid BR_EDRHandoverData;
    static const Uuid BSSControlPoint;
    static const Uuid BSSResponse;
    static const Uuid BarometricPressureTrend;
    static const Uuid BatteryCriticalStatus;
    static const Uuid BatteryEnergyStatus;
    static const Uuid BatteryHealthInformation;
    static const Uuid BatteryHealthStatus;
    static const Uuid BatteryInformation;
    static const Uuid BatteryLevel;
    static const Uuid BatteryLevelStatus;
    static const Uuid BatteryTimeStatus;
    static const Uuid BearerListCurrentCalls;
    static const Uuid BearerProviderName;
    static const Uuid BearerSignalStrength;
    static const Uuid BearerSignalStrengthReportingInterval;
    static const Uuid BearerTechnology;
    static const Uuid BearerUCI;
    static const Uuid BearerURISchemesSupportedList;
    static const Uuid BloodPressureFeature;
    static const Uuid BloodPressureMeasurement;
    static const Uuid BloodPressureRecord;
    static const Uuid BluetoothSIGData;
    static const Uuid BodyCompositionFeature;
    static const Uuid BodyCompositionMeasurement;
    static const Uuid BodySensorLocation;
    static const Uuid BondManagementControlPoint;
    static const Uuid BondManagementFeature;
    static const Uuid Boolean;
    static const Uuid BootKeyboardInputReport;
    static const Uuid BootKeyboardOutputReport;
    static const Uuid BootMouseInputReport;
    static const Uuid BroadcastAudioScanControlPoint;
    static const Uuid BroadcastReceiveState;
    static const Uuid CGMFeature;
    static const Uuid CGMMeasurement;
    static const Uuid CGMSessionRunTime;
    static const Uuid CGMSessionStartTime;
    static const Uuid CGMSpecificOpsControlPoint;
    static const Uuid CGMStatus;
    static const Uuid CIE13_3_1995ColorRenderingIndex;
    static const Uuid CO2_Concentration;
    static const Uuid CSCFeature;
    static const Uuid CSCMeasurement;
    static const Uuid CallControlPoint;
    static const Uuid CallControlPointOptionalOpcodes;
    static const Uuid CallFriendlyName;
    static const Uuid CallState;
    static const Uuid CaloricIntake;
    static const Uuid CarbonMonoxideConcentration;
    static const Uuid CardioRespiratoryActivityInstantaneousData;
    static const Uuid CardioRespiratoryActivitySummaryData;
    static const Uuid CentralAddressResolution;
    static const Uuid ChromaticDistancefromPlanckian;
    static const Uuid ChromaticityCoordinate;
    static const Uuid ChromaticityCoordinates;
    static const Uuid ChromaticityTolerance;
    static const Uuid ChromaticityinCCTandDuvValues;
    static const Uuid ClientSupportedFeatures;
    static const Uuid Coefficient;
    static const Uuid ConstantToneExtensionEnable;
    static const Uuid ContentControlID;
    static const Uuid CoordinatedSetSize;
    static const Uuid CorrelatedColorTemperature;
    static const Uuid CosineoftheAngle;
    static const Uuid Count16;
    static const Uuid Count24;
    static const Uuid CountryCode;
    static const Uuid CrossTrainerData;
    static const Uuid CurrentElapsedTime;
    static const Uuid CurrentGroupObjectID;
    static const Uuid CurrentTime;
    static const Uuid CurrentTrackObjectID;
    static const Uuid CurrentTrackSegmentsObjectID;
    static const Uuid CyclingPowerControlPoint;
    static const Uuid CyclingPowerFeature;
    static const Uuid CyclingPowerMeasurement;
    static const Uuid CyclingPowerVector;
    static const Uuid DSTOffset;
    static const Uuid DatabaseChangeIncrement;
    static const Uuid DatabaseHash;
    static const Uuid DateTime;
    static const Uuid DateUTC;
    static const Uuid DateofBirth;
    static const Uuid DateofThresholdAssessment;
    static const Uuid DayDateTime;
    static const Uuid DayofWeek;
    static const Uuid DescriptorValueChanged;
    static const Uuid DeviceName;
    static const Uuid DeviceTime;
    static const Uuid DeviceTimeControlPoint;
    static const Uuid DeviceTimeFeature;
    static const Uuid DeviceTimeParameters;
    static const Uuid DeviceWearingPosition;
    static const Uuid DewPoint;
    static const Uuid ESLAddress;
    static const Uuid ESLControlPoint;
    static const Uuid ESLCurrentAbsoluteTime;
    static const Uuid ESLDisplayInformation;
    static const Uuid ESLImageInformation;
    static const Uuid ESLLEDInformation;
    static const Uuid ESLResponseKeyMaterial;
    static const Uuid ESLSensorInformation;
    static const Uuid ElectricCurrent;
    static const Uuid ElectricCurrentRange;
    static const Uuid ElectricCurrentSpecification;
    static const Uuid ElectricCurrentStatistics;
    static const Uuid Elevation;
    static const Uuid EmailAddress;
    static const Uuid EmergencyID;
    static const Uuid EmergencyText;
    static const Uuid EncryptedDataKeyMaterial;
    static const Uuid Energy;
    static const Uuid Energy32;
    static const Uuid EnergyinaPeriodofDay;
    static const Uuid EnhancedBloodPressureMeasurement;
    static const Uuid EnhancedIntermediateCuffPressure;
    static const Uuid EstimatedServiceDate;
    static const Uuid EventStatistics;
    static const Uuid ExactTime256;
    static const Uuid FatBurnHeartRateLowerLimit;
    static const Uuid FatBurnHeartRateUpperLimit;
    static const Uuid FirmwareRevisionString;
    static const Uuid FirstName;
    static const Uuid FirstUseDate;
    static const Uuid FitnessMachineControlPoint;
    static const Uuid FitnessMachineFeature;
    static const Uuid FitnessMachineStatus;
    static const Uuid FiveZoneHeartRateLimits;
    static const Uuid FixedString16;
    static const Uuid FixedString24;
    static const Uuid FixedString36;
    static const Uuid FixedString64;
    static const Uuid FixedString8;
    static const Uuid FloorNumber;
    static const Uuid Force;
    static const Uuid FourZoneHeartRateLimits;
    static const Uuid GHSControlPoint;
    static const Uuid GMAPRole;
    static const Uuid GainSettingsAttribute;
    static const Uuid Gender;
    static const Uuid GeneralActivityInstantaneousData;
    static const Uuid GeneralActivitySummaryData;
    static const Uuid GenericLevel;
    static const Uuid GlobalTradeItemNumber;
    static const Uuid GlucoseFeature;
    static const Uuid GlucoseMeasurement;
    static const Uuid GlucoseMeasurementContext;
    static const Uuid GustFactor;
    static const Uuid HIDControlPoint;
    static const Uuid HIDInformation;
    static const Uuid HTTPControlPoint;
    static const Uuid HTTPEntityBody;
    static const Uuid HTTPHeaders;
    static const Uuid HTTPStatusCode;
    static const Uuid HTTPSSecurity;
    static const Uuid Handedness;
    static const Uuid HardwareRevisionString;
    static const Uuid HealthSensorFeatures;
    static const Uuid HearingAidFeatures;
    static const Uuid HearingAidPresetControlPoint;
    static const Uuid HeartRateControlPoint;
    static const Uuid HeartRateMax;
    static const Uuid HeartRateMeasurement;
    static const Uuid HeatIndex;
    static const Uuid Height;
    static const Uuid HighIntensityExerciseThreshold;
    static const Uuid HighResolutionHeight;
    static const Uuid HighTemperature;
    static const Uuid HighVoltage;
    static const Uuid HipCircumference;
    static const Uuid Humidity;
    static const Uuid IDDAnnunciationStatus;
    static const Uuid IDDCommandControlPoint;
    static const Uuid IDDCommandData;
    static const Uuid IDDFeatures;
    static const Uuid IDDHistoryData;
    static const Uuid IDDRecordAccessControlPoint;
    static const Uuid IDDStatus;
    static const Uuid IDDStatusChanged;
    static const Uuid IDDStatusReaderControlPoint;
    static const Uuid IEEE11073_20601RegulatoryCertificationDataList;
    static const Uuid IMDControl;
    static const Uuid IMDHistoricalData;
    static const Uuid IMDStatus;
    static const Uuid IMDSDescriptorValueChanged;
    static const Uuid Illuminance;
    static const Uuid IncomingCall;
    static const Uuid IncomingCallTargetBearerURI;
    static const Uuid IndoorBikeData;
    static const Uuid IndoorPositioningConfiguration;
    static const Uuid IntermediateCuffPressure;
    static const Uuid IntermediateTemperature;
    static const Uuid Irradiance;
    static const Uuid LEGATTSecurityLevels;
    static const Uuid LNControlPoint;
    static const Uuid LNFeature;
    static const Uuid Language;
    static const Uuid LastName;
    static const Uuid Latitude;
    static const Uuid Length;
    static const Uuid LifeCycleData;
    static const Uuid LightDistribution;
    static const Uuid LightOutput;
    static const Uuid LightSourceType;
    static const Uuid LinearPosition;
    static const Uuid LiveHealthObservations;
    static const Uuid LocalEastCoordinate;
    static const Uuid LocalNorthCoordinate;
    static const Uuid LocalTimeInformation;
    static const Uuid LocationName;
    static const Uuid LocationandSpeed;
    static const Uuid Longitude;
    static const Uuid LuminousEfficacy;
    static const Uuid LuminousEnergy;
    static const Uuid LuminousExposure;
    static const Uuid LuminousFlux;
    static const Uuid LuminousFluxRange;
    static const Uuid LuminousIntensity;
    static const Uuid MagneticDeclination;
    static const Uuid MagneticFluxDensity_2D;
    static const Uuid MagneticFluxDensity_3D;
    static const Uuid ManufacturerNameString;
    static const Uuid MassFlow;
    static const Uuid MaximumRecommendedHeartRate;
    static const Uuid MeasurementInterval;
    static const Uuid MediaControlPoint;
    static const Uuid MediaControlPointOpcodesSupported;
    static const Uuid MediaPlayerIconObjectID;
    static const Uuid MediaPlayerIconURL;
    static const Uuid MediaPlayerName;
    static const Uuid MediaState;
    static const Uuid MeshProvisioningDataIn;
    static const Uuid MeshProvisioningDataOut;
    static const Uuid MeshProxyDataIn;
    static const Uuid MeshProxyDataOut;
    static const Uuid MethaneConcentration;
    static const Uuid MiddleName;
    static const Uuid ModelNumberString;
    static const Uuid Mute;
    static const Uuid Navigation;
    static const Uuid NewAlert;
    static const Uuid NextTrackObjectID;
    static const Uuid NitrogenDioxideConcentration;
    static const Uuid Noise;
    static const Uuid Non_MethaneVolatileOrganicCompoundsConcentration;
    static const Uuid OTSFeature;
    static const Uuid ObjectActionControlPoint;
    static const Uuid ObjectChanged;
    static const Uuid ObjectFirst_Created;
    static const Uuid ObjectID;
    static const Uuid ObjectLast_Modified;
    static const Uuid ObjectListControlPoint;
    static const Uuid ObjectListFilter;
    static const Uuid ObjectName;
    static const Uuid ObjectProperties;
    static const Uuid ObjectSize;
    static const Uuid ObjectType;
    static const Uuid ObservationScheduleChanged;
    static const Uuid On_demandRangingData;
    static const Uuid OzoneConcentration;
    static const Uuid PLXContinuousMeasurement;
    static const Uuid PLXFeatures;
    static const Uuid PLXSpot_CheckMeasurement;
    static const Uuid ParentGroupObjectID;
    static const Uuid ParticulateMatter_PM1Concentration;
    static const Uuid ParticulateMatter_PM10Concentration;
    static const Uuid ParticulateMatter_PM2_5Concentration;
    static const Uuid PerceivedLightness;
    static const Uuid Percentage8;
    static const Uuid Percentage8Steps;
    static const Uuid PeripheralPreferredConnectionParameters;
    static const Uuid PeripheralPrivacyFlag;
    static const Uuid PhysicalActivityCurrentSession;
    static const Uuid PhysicalActivityMonitorControlPoint;
    static const Uuid PhysicalActivityMonitorFeatures;
    static const Uuid PhysicalActivitySessionDescriptor;
    static const Uuid PlaybackSpeed;
    static const Uuid PlayingOrder;
    static const Uuid PlayingOrdersSupported;
    static const Uuid PnPID;
    static const Uuid PollenConcentration;
    static const Uuid PositionQuality;
    static const Uuid Power;
    static const Uuid PowerSpecification;
    static const Uuid PreferredUnits;
    static const Uuid Pressure;
    static const Uuid ProtocolMode;
    static const Uuid RASControlPoint;
    static const Uuid RASFeatures;
    static const Uuid RCFeature;
    static const Uuid RCSettings;
    static const Uuid RSCFeature;
    static const Uuid RSCMeasurement;
    static const Uuid Rainfall;
    static const Uuid RangingDataOverwritten;
    static const Uuid RangingDataReady;
    static const Uuid Real_timeRangingData;
    static const Uuid ReconnectionAddress;
    static const Uuid ReconnectionConfigurationControlPoint;
    static const Uuid RecordAccessControlPoint;
    static const Uuid ReferenceTimeInformation;
    static const Uuid RegisteredUser;
    static const Uuid RelativeRuntimeinaCorrelatedColorTemperatureRange;
    static const Uuid RelativeRuntimeinaCurrentRange;
    static const Uuid RelativeRuntimeinaGenericLevelRange;
    static const Uuid RelativeValueinaPeriodofDay;
    static const Uuid RelativeValueinaTemperatureRange;
    static const Uuid RelativeValueinaVoltageRange;
    static const Uuid RelativeValueinanIlluminanceRange;
    static const Uuid Report;
    static const Uuid ReportMap;
    static const Uuid ResolvablePrivateAddressOnly;
    static const Uuid RestingHeartRate;
    static const Uuid RingerControlPoint;
    static const Uuid RingerSetting;
    static const Uuid RotationalSpeed;
    static const Uuid RowerData;
    static const Uuid SCControlPoint;
    static const Uuid ScanIntervalWindow;
    static const Uuid ScanRefresh;
    static const Uuid SearchControlPoint;
    static const Uuid SearchResultsObjectID;
    static const Uuid SedentaryIntervalNotification;
    static const Uuid SeekingSpeed;
    static const Uuid SensorLocation;
    static const Uuid SerialNumberString;
    static const Uuid ServerSupportedFeatures;
    static const Uuid ServiceChanged;
    static const Uuid ServiceCycleData;
    static const Uuid SetIdentityResolvingKey;
    static const Uuid SetMemberLock;
    static const Uuid SetMemberRank;
    static const Uuid SinkASE;
    static const Uuid SinkAudioLocations;
    static const Uuid SinkPAC;
    static const Uuid SleepActivityInstantaneousData;
    static const Uuid SleepActivitySummaryData;
    static const Uuid SoftwareRevisionString;
    static const Uuid SourceASE;
    static const Uuid SourceAudioLocations;
    static const Uuid SourcePAC;
    static const Uuid SportTypeforAerobicandAnaerobicThresholds;
    static const Uuid StairClimberData;
    static const Uuid StatusFlags;
    static const Uuid StepClimberData;
    static const Uuid StepCounterActivitySummaryData;
    static const Uuid StoredHealthObservations;
    static const Uuid StrideLength;
    static const Uuid SulfurDioxideConcentration;
    static const Uuid SulfurHexafluorideConcentration;
    static const Uuid SupportedAudioContexts;
    static const Uuid SupportedHeartRateRange;
    static const Uuid SupportedInclinationRange;
    static const Uuid SupportedNewAlertCategory;
    static const Uuid SupportedPowerRange;
    static const Uuid SupportedResistanceLevelRange;
    static const Uuid SupportedSpeedRange;
    static const Uuid SupportedUnreadAlertCategory;
    static const Uuid SystemID;
    static const Uuid TDSControlPoint;
    static const Uuid TMAPRole;
    static const Uuid Temperature;
    static const Uuid Temperature8;
    static const Uuid Temperature8Statistics;
    static const Uuid Temperature8inaPeriodofDay;
    static const Uuid TemperatureMeasurement;
    static const Uuid TemperatureRange;
    static const Uuid TemperatureStatistics;
    static const Uuid TemperatureType;
    static const Uuid TerminationReason;
    static const Uuid ThreeZoneHeartRateLimits;
    static const Uuid TimeAccuracy;
    static const Uuid TimeChangeLogData;
    static const Uuid TimeDecihour8;
    static const Uuid TimeExponential8;
    static const Uuid TimeHour24;
    static const Uuid TimeMillisecond24;
    static const Uuid TimeSecond16;
    static const Uuid TimeSecond32;
    static const Uuid TimeSecond8;
    static const Uuid TimeSource;
    static const Uuid TimeUpdateControlPoint;
    static const Uuid TimeUpdateState;
    static const Uuid TimeZone;
    static const Uuid TimewithDST;
    static const Uuid Torque;
    static const Uuid TrackChanged;
    static const Uuid TrackDuration;
    static const Uuid TrackPosition;
    static const Uuid TrackTitle;
    static const Uuid TrainingStatus;
    static const Uuid TreadmillData;
    static const Uuid TrueWindDirection;
    static const Uuid TrueWindSpeed;
    static const Uuid TwoZoneHeartRateLimits;
    static const Uuid TxPowerLevel;
    static const Uuid UDIforMedicalDevices;
    static const Uuid UGGFeatures;
    static const Uuid UGTFeatures;
    static const Uuid URI;
    static const Uuid UVIndex;
    static const Uuid Uncertainty;
    static const Uuid UnreadAlertStatus;
    static const Uuid UserControlPoint;
    static const Uuid UserIndex;
    static const Uuid VO2Max;
    static const Uuid VOCConcentration;
    static const Uuid Voltage;
    static const Uuid VoltageFrequency;
    static const Uuid VoltageSpecification;
    static const Uuid VoltageStatistics;
    static const Uuid VolumeControlPoint;
    static const Uuid VolumeFlags;
    static const Uuid VolumeFlow;
    static const Uuid VolumeOffsetControlPoint;
    static const Uuid VolumeOffsetState;
    static const Uuid VolumeState;
    static const Uuid WaistCircumference;
    static const Uuid Weight;
    static const Uuid WeightMeasurement;
    static const Uuid WeightScaleFeature;
    static const Uuid WindChill;
    static const Uuid WorkCycleData;
  };

  struct ServiceClasses {
    static const Uuid ServiceDiscoveryServerServiceClassID;
    static const Uuid BrowseGroupDescriptorServiceClassID;
    static const Uuid SerialPort;
    static const Uuid LANAccessUsingPPP;
    static const Uuid DialUpNetworking;
    static const Uuid IrMCSync;
    static const Uuid OBEXObjectPush;
    static const Uuid OBEXFileTransfer;
    static const Uuid IrMCSyncCommand;
    static const Uuid Headset;
    static const Uuid CordlessTelephony;
    static const Uuid AudioSource;
    static const Uuid AudioSink;
    static const Uuid AVRemoteControlTarget;
    static const Uuid AdvancedAudioDistribution;
    static const Uuid AVRemoteControl;
    static const Uuid AVRemoteControlController;
    static const Uuid Intercom;
    static const Uuid Fax;
    static const Uuid HeadsetAudioGateway;
    static const Uuid WAP;
    static const Uuid WAP_CLIENT;
    static const Uuid PANU;
    static const Uuid NAP;
    static const Uuid GN;
    static const Uuid DirectPrinting;
    static const Uuid ReferencePrinting;
    static const Uuid Imaging;
    static const Uuid ImagingResponder;
    static const Uuid ImagingAutomaticArchive;
    static const Uuid ImagingReferencedObjects;
    static const Uuid HandsFree;
    static const Uuid AGHandsFree;
    static const Uuid DirectPrintingReferencedObjectsService;
    static const Uuid ReflectedUI;
    static const Uuid BasicPrinting;
    static const Uuid PrintingStatus;
    static const Uuid HID;
    static const Uuid HardcopyCableReplacement;
    static const Uuid HCR_Print;
    static const Uuid HCR_Scan;
    static const Uuid Common_ISDN_Access;
    static const Uuid SIMAccess;
    static const Uuid PhonebookAccessClient;
    static const Uuid PhonebookAccessServer;
    static const Uuid PhonebookAccessProfile;
    static const Uuid HeadsetHS;
    static const Uuid MessageAccessServer;
    static const Uuid MessageNotificationServer;
    static const Uuid MessageAccessProfile;
    static const Uuid GNSS;
    static const Uuid GNSS_Server;
    static const Uuid Display3D;
    static const Uuid Glasses3D;
    static const Uuid SynchProfile3D;
    static const Uuid MultiProfileSpecification;
    static const Uuid MPS;
    static const Uuid CTNAccessService;
    static const Uuid CTNNotificationService;
    static const Uuid CalendarTasksandNotesProfile;
    static const Uuid PnPInformation;
    static const Uuid GenericNetworking;
    static const Uuid GenericFileTransfer;
    static const Uuid GenericAudio;
    static const Uuid GenericTelephony;
    static const Uuid UPNP_Service;
    static const Uuid UPNP_IP_Service;
    static const Uuid ESDP_UPNP_IP_PAN;
    static const Uuid ESDP_UPNP_IP_LAP;
    static const Uuid ESDP_UPNP_L2CAP;
    static const Uuid VideoSource;
    static const Uuid VideoSink;
    static const Uuid VideoDistribution;
    static const Uuid HDP;
    static const Uuid HDPSource;
    static const Uuid HDPSink;
  };
  /**
   * @brief Checks if the UUID is a well-known Bluetooth UUID.
   * @return True if the UUID is well-known; otherwise, false.
   */
  bool isWellKnown() const;

 private:
  unsigned char m_uuid[16];
};

}  // namespace bluetooth
