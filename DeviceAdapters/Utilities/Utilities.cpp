///////////////////////////////////////////////////////////////////////////////
// FILE:          Utilities.cpp
// PROJECT:       Micro-Manager
// SUBSYSTEM:     DeviceAdapters
//-----------------------------------------------------------------------------
// DESCRIPTION:   Various 'Meta-Devices' that add to or combine functionality of 
//                physcial devices.
//
// AUTHOR:        Nico Stuurman, nico@cmp.ucsf.edu, 11/07/2008
//                DAXYStage by Ed Simmon, 11/28/2011
// COPYRIGHT:     University of California, San Francisco, 2008
// LICENSE:       This file is distributed under the BSD license.
//                License text is included with the source distribution.
//
//                This file is distributed in the hope that it will be useful,
//                but WITHOUT ANY WARRANTY; without even the implied warranty
//                of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
//
//                IN NO EVENT SHALL THE COPYRIGHT OWNER OR
//                CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
//                INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES.
//

#include "Utilities.h"
#include <algorithm>
#include "../../MMDevice/ModuleInterface.h"
#include "../../MMDevice/MMDevice.h"

#ifdef WIN32
   #define WIN32_LEAN_AND_MEAN
   #include <windows.h>
   #define snprintf _snprintf 
#endif


const char* g_Undefined = "Undefined";
const char* g_NoDevice = "None";
const char* g_DeviceNameMultiShutter = "Multi Shutter";
const char* g_DeviceNameMultiCamera = "Multi Camera";
const char* g_DeviceNameDAShutter = "DA Shutter";
const char* g_DeviceNameDAZStage = "DA Z Stage";
const char* g_DeviceNameDAXYStage = "DA XY Stage";
const char* g_DeviceNameAutoFocusStage = "AutoFocus Stage";
const char* g_DeviceNameStateDeviceShutter = "State Device Shutter";

const char* g_PropertyMinUm = "Stage Low Position(um)";
const char* g_PropertyMaxUm = "Stage High Position(um)";

///////////////////////////////////////////////////////////////////////////////
// Exported MMDevice API
///////////////////////////////////////////////////////////////////////////////
MODULE_API void InitializeModuleData()
{
   AddAvailableDeviceName(g_DeviceNameMultiShutter, "Combine multiple physical shutters into a single logical shutter");
   AddAvailableDeviceName(g_DeviceNameMultiCamera, "Combine multiple physical cameras into a single logical camera");
   AddAvailableDeviceName(g_DeviceNameDAShutter, "DA used as a shutter");
   AddAvailableDeviceName(g_DeviceNameDAZStage, "DA-controlled Z-stage");
   AddAvailableDeviceName(g_DeviceNameDAXYStage, "DA-controlled XY-stage");
   AddAvailableDeviceName(g_DeviceNameAutoFocusStage, "AutoFocus offset acting as a Z-stage");
   AddAvailableDeviceName(g_DeviceNameStateDeviceShutter, "State device used as a shutter");
}

MODULE_API MM::Device* CreateDevice(const char* deviceName)                  
{
   if (deviceName == 0)
      return 0;

   if (strcmp(deviceName, g_DeviceNameMultiShutter) == 0) { 
      return new MultiShutter();
   } else if (strcmp(deviceName, g_DeviceNameMultiCamera) == 0) { 
      return new MultiCamera();
   } else if (strcmp(deviceName, g_DeviceNameDAShutter) == 0) { 
      return new DAShutter();
   } else if (strcmp(deviceName, g_DeviceNameDAZStage) == 0) { 
      return new DAZStage();
   } else if (strcmp(deviceName, g_DeviceNameDAXYStage) == 0) { 
      return new DAXYStage();
   } else if (strcmp(deviceName, g_DeviceNameAutoFocusStage) == 0) { 
      return new AutoFocusStage();
   } else if (strcmp(deviceName, g_DeviceNameStateDeviceShutter) == 0) {
      return new StateDeviceShutter();
   }

   return 0;
}

MODULE_API void DeleteDevice(MM::Device* pDevice)                            
{                                                                            
   delete pDevice;                                                           
}


///////////////////////////////////////////////////////////////////////////////
// Multi Shutter implementation
///////////////////////////////////////////////////////////////////////////////
MultiShutter::MultiShutter() :
   nrPhysicalShutters_(5), // determines how many slots for shutters we have
   open_(false),
   initialized_ (false)
{
   InitializeDefaultErrorMessages();

   SetErrorText(ERR_INVALID_DEVICE_NAME, "Please select a valid shutter");

   // Name                                                                   
   CreateProperty(MM::g_Keyword_Name, g_DeviceNameMultiShutter, MM::String, true); 
                                                                             
   // Description                                                            
   CreateProperty(MM::g_Keyword_Description, "Combines multiple physical shutters into a single ", MM::String, true);

   for (int i = 0; i < nrPhysicalShutters_; i++) {
      usedShutters_.push_back(g_Undefined);
      physicalShutters_.push_back(0);
   }
}
 
MultiShutter::~MultiShutter()
{
   Shutdown();
}

void MultiShutter::GetName(char* name) const
{
   CDeviceUtils::CopyLimitedString(name, g_DeviceNameMultiShutter);
}                                                                            
                                                                             
int MultiShutter::Initialize() 
{
   // get list with available Shutters.   
   // TODO: this is a initialization parameter, which makes it harder for the end-user to set up!
   std::vector<std::string> availableShutters;
   availableShutters.clear();
   char deviceName[MM::MaxStrLength];
   unsigned int deviceIterator = 0;
   for(;;)
   {
      GetLoadedDeviceOfType(MM::ShutterDevice, deviceName, deviceIterator++);
      if( 0 < strlen(deviceName))
      {
         availableShutters.push_back(std::string(deviceName));
      }
      else
         break;
   }

   availableShutters_.push_back(g_Undefined);
   std::vector<std::string>::iterator iter;
   for (iter = availableShutters.begin(); iter != availableShutters.end(); iter++ ) {
      MM::Device* shutter = GetDevice((*iter).c_str());
      std::ostringstream os;
      os << this << " " << shutter;
      LogMessage(os.str().c_str());
      if (shutter &&  (this != shutter))
         availableShutters_.push_back(*iter);
   }

   for (long i = 0; i < nrPhysicalShutters_; i++) {
      CPropertyActionEx* pAct = new CPropertyActionEx (this, &MultiShutter::OnPhysicalShutter, i);
      std::ostringstream os;
      os << "Physical Shutter " << i+1;
      CreateProperty(os.str().c_str(), availableShutters_[0].c_str(), MM::String, false, pAct, false);
      SetAllowedValues(os.str().c_str(), availableShutters_);
   }


   CPropertyAction* pAct = new CPropertyAction(this, &MultiShutter::OnState);
   CreateProperty("State", "0", MM::Integer, false, pAct);
   AddAllowedValue("State", "0");
   AddAllowedValue("State", "1");

   int ret = UpdateStatus();
   if (ret != DEVICE_OK)
      return ret;

   initialized_ = true;

   return DEVICE_OK;
}

bool MultiShutter::Busy()
{
   std::vector<MM::Shutter*>::iterator iter;
   for (iter = physicalShutters_.begin(); iter != physicalShutters_.end(); iter++ ) {
      if ( (*iter != 0) && (*iter)->Busy())
         return true;
   }

   return false;
}

/*
 * Opens or closes all physical shutters.
 */
int MultiShutter::SetOpen(bool open)
{
   std::vector<MM::Shutter*>::iterator iter;
   for (iter = physicalShutters_.begin(); iter != physicalShutters_.end(); iter++ ) {
      if (*iter != 0) {
         int ret = (*iter)->SetOpen(open);
         if (ret != DEVICE_OK)
            return ret;
      }
   }
   open_ = open;
   return DEVICE_OK;
}

///////////////////////////////////////
// Action Interface
//////////////////////////////////////
int MultiShutter::OnPhysicalShutter(MM::PropertyBase* pProp, MM::ActionType eAct, long i)
{
   if (eAct == MM::BeforeGet)
   {
      pProp->Set(usedShutters_[i].c_str());
   }
   else if (eAct == MM::AfterSet)
   {
      std::string shutterName;
      pProp->Get(shutterName);
      if (shutterName == g_Undefined) {
         usedShutters_[i] = g_Undefined;
         physicalShutters_[i] = 0;
      } else {
         MM::Shutter* shutter = (MM::Shutter*) GetDevice(shutterName.c_str());
         if (shutter != 0) {
            usedShutters_[i] = shutterName;
            physicalShutters_[i] = shutter;
         } else
            return ERR_INVALID_DEVICE_NAME;
      }
   }

   return DEVICE_OK;
}


int MultiShutter::OnState(MM::PropertyBase* pProp, MM::ActionType eAct)
{ 
   if (eAct == MM::BeforeGet)
   {
      bool open;
      int ret = GetOpen(open);
      if (ret != DEVICE_OK)
         return ret;
      long state = 0;
      if (open)
         state = 1;
      pProp->Set(state);
   }
   else if (eAct == MM::AfterSet)
   {
      long state;
      pProp->Get(state);
      bool open = false;
      if (state == 1)
         open = true;
      SetOpen(open);
   }
   return DEVICE_OK;
}

///////////////////////////////////////////////////////////////////////////////
// Multi Shutter implementation
///////////////////////////////////////////////////////////////////////////////
MultiCamera::MultiCamera() :
   bufferSize_(0),
   imageBuffer_(0),
   nrCamerasInUse_(0),
   initialized_(false)
{
   InitializeDefaultErrorMessages();

   SetErrorText(ERR_INVALID_DEVICE_NAME, "Please select a valid camera");

   // Name                                                                   
   CreateProperty(MM::g_Keyword_Name, g_DeviceNameMultiCamera, MM::String, true); 
                                                                             
   // Description                                                            
   CreateProperty(MM::g_Keyword_Description, "Combines multiple cameras into a single camera", MM::String, true);

   for (int i = 0; i < MAX_NUMBER_PHYSICAL_CAMERAS; i++) {
      usedCameras_.push_back(g_Undefined);
      physicalCameras_.push_back(0);
   }
}

MultiCamera::~MultiCamera()
{
   if (initialized_)
      Shutdown();
}

int MultiCamera::Shutdown()
{
   delete imageBuffer_;
   // Rely on the cameras to shut themselves down
   return DEVICE_OK;
}

int MultiCamera::Initialize()
{
   // get list with available Cameras.   
   std::vector<std::string> availableCameras;
   availableCameras.clear();
   char deviceName[MM::MaxStrLength];
   unsigned int deviceIterator = 0;
   for(;;)
   {
      GetLoadedDeviceOfType(MM::CameraDevice, deviceName, deviceIterator++);
      if( 0 < strlen(deviceName))
      {
         availableCameras.push_back(std::string(deviceName));
      }
      else
         break;
   }

   availableCameras_.push_back(g_Undefined);
   std::vector<std::string>::iterator iter;
   for (iter = availableCameras.begin(); 
         iter != availableCameras.end(); 
         iter++ ) 
   {
      MM::Device* camera = GetDevice((*iter).c_str());
      std::ostringstream os;
      os << this << " " << camera;
      LogMessage(os.str().c_str());
      if (camera &&  (this != camera))
         availableCameras_.push_back(*iter);
   }

   for (long i = 0; i < MAX_NUMBER_PHYSICAL_CAMERAS; i++) 
   {
      CPropertyActionEx* pAct = new CPropertyActionEx (this, &MultiCamera::OnPhysicalCamera, i);
      std::ostringstream os;
      os << "Physical Camera " << i+1;
      CreateProperty(os.str().c_str(), availableCameras_[0].c_str(), MM::String, false, pAct, false);
      SetAllowedValues(os.str().c_str(), availableCameras_);
   }

   CPropertyAction* pAct = new CPropertyAction(this, &MultiCamera::OnBinning);
   CreateProperty(MM::g_Keyword_Binning, "1", MM::Integer, false, pAct, false);

   initialized_ = true;

   return DEVICE_OK;
}

void MultiCamera::GetName(char* name) const
{
   CDeviceUtils::CopyLimitedString(name, g_DeviceNameMultiCamera);
}

int MultiCamera::SnapImage()
{
   CameraSnapThread t[MAX_NUMBER_PHYSICAL_CAMERAS];
   for (unsigned int i = 0; i < physicalCameras_.size(); i++)
   {
      if (physicalCameras_[i] != 0) 
      {
         t[i].SetCamera(physicalCameras_[i]);
         t[i].Start();
      }
   }
   // I think that the CameraSnapThread destructor waits until the SnapImage function is done
   // So, we are likely to be waiting here until all cameras are done snapping

   return DEVICE_OK;
}

/**
 * return the ImageBuffer of the first physical camera
 */
const unsigned char* MultiCamera::GetImageBuffer()
{
   return GetImageBuffer(0);
}

const unsigned char* MultiCamera::GetImageBuffer(unsigned channelNr)
{
   // We have a vector of physicalCameras, and a vector of Strings listing the cameras
   // we actually use.  
   int j = -1;
   unsigned height = GetImageHeight();
   unsigned width = GetImageWidth();
   unsigned pixDepth = GetImageBytesPerPixel();
   for (unsigned int i = 0; i < physicalCameras_.size(); i++)
   {
      if (usedCameras_[i] != g_Undefined)
         j++;
      if (j == (int) channelNr)
      {
         unsigned thisHeight = physicalCameras_[i]->GetImageHeight();
         unsigned thisWidth = physicalCameras_[i]->GetImageWidth();
         if (height == thisHeight && width == thisWidth)
            return physicalCameras_[i]->GetImageBuffer();
         else
         {
            img_.Resize(width, height, pixDepth);
            img_.ResetPixels();
            if (width == thisWidth)
            {
               memcpy(img_.GetPixelsRW(), physicalCameras_[i]->GetImageBuffer(), thisHeight * thisWidth * pixDepth);
            }
            else 
            {
               // we need to copy line by line
               const unsigned char* pixels = physicalCameras_[i]->GetImageBuffer();
               for (unsigned i=0; i < thisHeight; i++)
               {
                  memcpy(img_.GetPixelsRW() + i * width, pixels + i * thisWidth, thisWidth);
               }
            }
            return img_.GetPixels();
         }
      }
   }
   return 0;
}

bool MultiCamera::IsCapturing()
{
   std::vector<MM::Camera*>::iterator iter;
   for (iter = physicalCameras_.begin(); iter != physicalCameras_.end(); iter++ ) {
      if ( (*iter != 0) && (*iter)->IsCapturing())
         return true;
   }

   return false;
}

/**
 * Returns the largest width of cameras used
 */
unsigned MultiCamera::GetImageWidth() const
{
   // TODO: should we use cached width?
   // If so, when do we cache?
   // Since this function is const, we can not cache the width found
   unsigned width = 0;
   unsigned int j = 0;
   while (j < physicalCameras_.size() ) 
   {
      if (physicalCameras_[j] != 0) {
         unsigned tmp = physicalCameras_[j]->GetImageWidth();
         if (tmp > width)
            width = tmp;
      }
      j++;
   }
  
   return width;
}

/**
 * Returns the largest height of cameras used
 */
unsigned MultiCamera::GetImageHeight() const
{
   unsigned height = 0;
   unsigned int j = 0;
   while (j < physicalCameras_.size() ) 
   {
      if (physicalCameras_[j] != 0)
      {
         unsigned tmp = physicalCameras_[j]->GetImageHeight();
         if (tmp > height)
            height = tmp;
      }
      j++;
   }
  
   return height;
}

unsigned MultiCamera::GetImageBytesPerPixel() const
{
   if (physicalCameras_[0] != 0)
   {
      unsigned bytes = physicalCameras_[0]->GetImageBytesPerPixel();
      for (unsigned int i = 1; i < physicalCameras_.size(); i++)
      {
         if (physicalCameras_[i] != 0) 
            if (bytes != physicalCameras_[i]->GetImageBytesPerPixel())
               return 0;
      }
      return bytes;
   }
   return 0;
}

unsigned MultiCamera::GetBitDepth() const
{
   if (physicalCameras_[0] != 0)
   {
      unsigned bitDepth = physicalCameras_[0]->GetBitDepth();
      for (unsigned int i = 1; i < physicalCameras_.size(); i++)
      {
         if (physicalCameras_[i] != 0) 
            if (bitDepth != physicalCameras_[i]->GetBitDepth())
               return 0;
      }
      return bitDepth;
   }
   return 0;
}

long MultiCamera::GetImageBufferSize() const
{
   long maxSize = 0;
   int unsigned counter = 0;
   for (unsigned int i = 0; i < physicalCameras_.size(); i++)
   {
      if (physicalCameras_[i] != 0) 
      {
         counter++;
         long tmp = physicalCameras_[i]->GetImageBufferSize();
         if (tmp > maxSize)
            maxSize = tmp;
      }
   }

   return counter * maxSize;
}

double MultiCamera::GetExposure() const
{
   if (physicalCameras_[0] != 0)
   {
      double exposure = physicalCameras_[0]->GetExposure();
      for (unsigned int i = 1; i < physicalCameras_.size(); i++)
      {
         if (physicalCameras_[i] != 0) 
            if (exposure != physicalCameras_[i]->GetExposure())
               return 0;
      }
      return exposure;
   }
   return 0.0;
}

void MultiCamera::SetExposure(double exp)
{
   if (exp > 0.0)
   {
      for (unsigned int i = 0; i < physicalCameras_.size(); i++)
      {
         if (physicalCameras_[i] != 0) 
           physicalCameras_[i]->SetExposure(exp);
      }
   }
}

int MultiCamera::SetROI(unsigned x, unsigned y, unsigned xSize, unsigned ySize)
{
   for (unsigned int i = 0; i < physicalCameras_.size(); i++)
   {
      // TODO: deal with case when CCD size are not identical
      if (physicalCameras_[i] != 0) 
      {
         int ret = physicalCameras_[i]->SetROI(x, y, xSize, ySize);
         if (ret != DEVICE_OK)
            return ret;
      }
   }
   return DEVICE_OK;
}

int MultiCamera::GetROI(unsigned& x, unsigned& y, unsigned& xSize, unsigned& ySize)
{
   // TODO: check if ROI is same on all cameras
   if (physicalCameras_[0] != 0)
   {
      int ret = physicalCameras_[0]->GetROI(x, y, xSize, ySize);
      if (ret != DEVICE_OK)
         return ret;
   }

   return DEVICE_OK;
}

int MultiCamera::ClearROI()
{
   for (unsigned int i = 0; i < physicalCameras_.size(); i++)
   {
      if (physicalCameras_[i] != 0)
      {
         int ret = physicalCameras_[i]->ClearROI();
         if (ret != DEVICE_OK)
            return ret;
      }
   }

   return DEVICE_OK;
}

int MultiCamera::PrepareSequenceAcqusition()
{
   for (unsigned int i = 0; i < physicalCameras_.size(); i++)
   {
      if (physicalCameras_[i] != 0)
      {
         int ret = physicalCameras_[i]->PrepareSequenceAcqusition();
         if (ret != DEVICE_OK)
            return ret;
      }
   }

   return DEVICE_OK;
}

int MultiCamera::StartSequenceAcquisition(double interval)
{
   for (unsigned int i = 0; i < physicalCameras_.size(); i++)
   {
      if (physicalCameras_[i] != 0)
      {
         int ret = physicalCameras_[i]->StartSequenceAcquisition(interval);
         if (ret != DEVICE_OK)
            return ret;
      }
   }
   return DEVICE_OK;
}

int MultiCamera::StartSequenceAcquisition(long numImages, double interval_ms, bool stopOnOverflow)
{
   for (unsigned int i = 0; i < physicalCameras_.size(); i++)
   {
      if (physicalCameras_[i] != 0)
      {
         int ret = physicalCameras_[i]->StartSequenceAcquisition(numImages, interval_ms, stopOnOverflow);
         if (ret != DEVICE_OK)
            return ret;
      }
   }
   return DEVICE_OK;
}

int MultiCamera::StopSequenceAcquisition()
{
   for (unsigned int i = 0; i < physicalCameras_.size(); i++)
   {
      if (physicalCameras_[i] != 0)
      {
         int ret = physicalCameras_[i]->StopSequenceAcquisition();
         if (ret != DEVICE_OK)
            return ret;
      }
   }
   return DEVICE_OK;
}

int MultiCamera::GetBinning() const
{
   int binning = 0;
   if (physicalCameras_[0] != 0)
      binning = physicalCameras_[0]->GetBinning();
   for (unsigned int i = 0; i < physicalCameras_.size(); i++)
   {
      if (physicalCameras_[i] != 0)
      {
         if (binning != physicalCameras_[i]->GetBinning())
            return 0;
      }
   }
   return binning;
}

int MultiCamera::SetBinning(int bS)
{
   for (unsigned int i = 0; i < physicalCameras_.size(); i++)
   {
      if (physicalCameras_[i] != 0)
      {
         int ret = physicalCameras_[i]->SetBinning(bS);
         if (ret != DEVICE_OK)
            return ret;
      }
   }
   return DEVICE_OK;
}

int MultiCamera::IsExposureSequenceable(bool& isSequenceable) const
{
   isSequenceable = false;

   return DEVICE_OK;
}

unsigned MultiCamera::GetNumberOfComponents() const
{
   return 1;
}

unsigned MultiCamera::GetNumberOfChannels() const
{
   return nrCamerasInUse_;
}

int MultiCamera::GetChannelName(unsigned channel, char* name)
{
   CDeviceUtils::CopyLimitedString(name, "");
   unsigned int ch = Logical2Physical(channel);
   if (ch >= 0 && ch < usedCameras_.size())
   {
      CDeviceUtils::CopyLimitedString(name, usedCameras_[ch].c_str());
   }
   return DEVICE_OK;
}

int MultiCamera::Logical2Physical(int logical)
{
   int j = -1;
   for (unsigned int i = 0; i < usedCameras_.size(); i++)
   {
      if (usedCameras_[i] != g_Undefined)
         j++;
      if (j == logical)
         return i;
   }
   return -1;
}
  

int MultiCamera::OnPhysicalCamera(MM::PropertyBase* pProp, MM::ActionType eAct, long i)
{
   if (eAct == MM::BeforeGet)
   {
      pProp->Set(usedCameras_[i].c_str());
   }

   else if (eAct == MM::AfterSet)
   {
      if (physicalCameras_[i] != 0)
      {
         physicalCameras_[i]->RemoveTag(MM::g_Keyword_CameraChannelName);
         physicalCameras_[i]->RemoveTag(MM::g_Keyword_CameraChannelIndex);
      }

      std::string cameraName;
      pProp->Get(cameraName);

      if (cameraName == g_Undefined) {
         usedCameras_[i] = g_Undefined;
         physicalCameras_[i] = 0;
      } else {
         MM::Camera* camera = (MM::Camera*) GetDevice(cameraName.c_str());
         if (camera != 0) {
            usedCameras_[i] = cameraName;
            physicalCameras_[i] = camera;
            std::ostringstream os;
            os << i;
            char myName[MM::MaxStrLength];
            GetLabel(myName);
            camera->AddTag(MM::g_Keyword_CameraChannelName, myName, usedCameras_[i].c_str());
            camera->AddTag(MM::g_Keyword_CameraChannelIndex, myName, os.str().c_str());
         } else
            return ERR_INVALID_DEVICE_NAME;
      }
      nrCamerasInUse_ = 0;
      for (unsigned int i = 0; i < usedCameras_.size(); i++) 
      {
         if (usedCameras_[i] != g_Undefined)
            nrCamerasInUse_++;
      }

      // TODO: Set allowed binning values correctly
      if (physicalCameras_[0] != 0)
      {
         ClearAllowedValues(MM::g_Keyword_Binning);
         int nr = physicalCameras_[0]->GetNumberOfPropertyValues(MM::g_Keyword_Binning);
         for (int j = 0; j < nr; j++)
         {
            char value[MM::MaxStrLength];
            physicalCameras_[0]->GetPropertyValueAt(MM::g_Keyword_Binning, j, value);
            AddAllowedValue(MM::g_Keyword_Binning, value);
         }
      }
   }

   return DEVICE_OK;
}

int MultiCamera::OnBinning(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      pProp->Set((long)GetBinning());
   }
   else if (eAct == MM::AfterSet)
   {
      long binning;
      pProp->Get(binning);
      int ret = SetBinning(binning);
      if (ret != DEVICE_OK)
         return ret;
   }
   return DEVICE_OK;
}


CameraSnapThread::~CameraSnapThread()
{
   wait();
}

int CameraSnapThread::svc()
{
   return camera_->SnapImage();
}

void CameraSnapThread::Start()
{
   activate();
}

/**********************************************************************
 * DAShutter implementation
 */
DAShutter::DAShutter() :
   DADevice_(0),
   DADeviceName_ (""),
   initialized_ (false)
{
   InitializeDefaultErrorMessages();

   SetErrorText(ERR_INVALID_DEVICE_NAME, "Please select a valid DA device");
   SetErrorText(ERR_NO_DA_DEVICE, "No DA Device selected");
   SetErrorText(ERR_NO_DA_DEVICE_FOUND, "No DA Device loaded");

   // Name                                                                   
   CreateProperty(MM::g_Keyword_Name, g_DeviceNameDAShutter, MM::String, true); 
                                                                             
   // Description                                                            
   CreateProperty(MM::g_Keyword_Description, "DA device that is used as a shutter", MM::String, true);

}  
 
DAShutter::~DAShutter()
{
   Shutdown();
}

void DAShutter::GetName(char* Name) const
{
   CDeviceUtils::CopyLimitedString(Name, g_DeviceNameDAShutter);
}                                                                            
                                                                             
int DAShutter::Initialize() 
{
   // get list with available DA devices.
   // TODO: this is a initialization parameter, which makes it harder for the end-user to set up!
   availableDAs_.clear();
   char deviceName[MM::MaxStrLength];
   unsigned int deviceIterator = 0;
   for(;;)
   {
      GetLoadedDeviceOfType(MM::SignalIODevice, deviceName, deviceIterator++);
      if( 0 < strlen(deviceName))
      {
         availableDAs_.push_back(std::string(deviceName));
      }
      else
         break;
   }


   CPropertyAction* pAct = new CPropertyAction (this, &DAShutter::OnDADevice);      
   std::string defaultDA = "Undefined";
   if (availableDAs_.size() >= 1)
      defaultDA = availableDAs_[0];
   CreateProperty("DA Device", defaultDA.c_str(), MM::String, false, pAct, false);         
   if (availableDAs_.size() >= 1)
      SetAllowedValues("DA Device", availableDAs_);
   else
      return ERR_NO_DA_DEVICE_FOUND;

   // This is needed, otherwise DeviceDA_ is not always set resulting in crashes
   // This could lead to strange problems if multiple DA devices are loaded
   SetProperty("DA Device", defaultDA.c_str());

   pAct = new CPropertyAction(this, &DAShutter::OnState);
   CreateProperty("State", "0", MM::Integer, false, pAct);
   AddAllowedValue("State", "0");
   AddAllowedValue("State", "1");

   int ret = UpdateStatus();
   if (ret != DEVICE_OK)
      return ret;

   initialized_ = true;

   return DEVICE_OK;
}

bool DAShutter::Busy()
{
   if (DADevice_ != 0)
      return DADevice_->Busy();

   // If we are here, there is a problem.  No way to report it.
   return false;
}

/*
 * Opens or closes the shutter.  Remembers voltage from the 'open' position
 */
int DAShutter::SetOpen(bool open)
{
   if (DADevice_ == 0)
      return ERR_NO_DA_DEVICE;

   return DADevice_->SetGateOpen(open);
}

int DAShutter::GetOpen(bool& open)
{
   if (DADevice_ == 0)
      return ERR_NO_DA_DEVICE;

   return DADevice_->GetGateOpen(open);
}

///////////////////////////////////////
// Action Interface
//////////////////////////////////////
int DAShutter::OnDADevice(MM::PropertyBase* pProp, MM::ActionType eAct)
{ 
   if (eAct == MM::BeforeGet)
   {
      pProp->Set(DADeviceName_.c_str());
   }
   else if (eAct == MM::AfterSet)
   {
      // Make sure that the "old" DA device is open:
      SetOpen(true);

      std::string DADeviceName;
      pProp->Get(DADeviceName);
      MM::SignalIO* DADevice = (MM::SignalIO*) GetDevice(DADeviceName.c_str());
      if (DADevice != 0) {
         DADevice_ = DADevice;
         DADeviceName_ = DADeviceName;
      } else
         return ERR_INVALID_DEVICE_NAME;

      // Gates are open by default.  Start with shutter closed:
      SetOpen(false);
   }
   return DEVICE_OK;
}


int DAShutter::OnState(MM::PropertyBase* pProp, MM::ActionType eAct)
{ 
   if (eAct == MM::BeforeGet)
   {
      bool open;
      int ret = GetOpen(open);
      if (ret != DEVICE_OK)
         return ret;
      long state = 0;
      if (open)
         state = 1;
      pProp->Set(state);
   }
   else if (eAct == MM::AfterSet)
   {
      long state;
      pProp->Get(state);
      bool open = false;
      if (state == 1)
         open = true;
      return SetOpen(open);
   }
   return DEVICE_OK;
}

/**************************
 * DAZStage implementation
 */

DAZStage::DAZStage() :
   DADeviceName_ (""),
   initialized_ (false),
   minDAVolt_ (0.0),
   maxDAVolt_ (10.0),
   minStageVolt_ (0.0),
   maxStageVolt_ (5.0),
   minStagePos_ (0.0),
   maxStagePos_ (200.0),
   pos_ (0.0),
   originPos_ (0.0)
{
   InitializeDefaultErrorMessages();

   SetErrorText(ERR_INVALID_DEVICE_NAME, "Please select a valid DA device");
   SetErrorText(ERR_NO_DA_DEVICE, "No DA Device selected");
   SetErrorText(ERR_VOLT_OUT_OF_RANGE, "The DA Device cannot set the requested voltage");
   SetErrorText(ERR_POS_OUT_OF_RANGE, "The requested position is out of range");
   SetErrorText(ERR_NO_DA_DEVICE_FOUND, "No DA Device loaded");

   // Name                                                                   
   CreateProperty(MM::g_Keyword_Name, g_DeviceNameDAZStage, MM::String, true); 
                                                                             
   // Description                                                            
   CreateProperty(MM::g_Keyword_Description, "ZStage controlled with voltage provided by a DA board", MM::String, true);

   CPropertyAction* pAct = new CPropertyAction (this, &DAZStage::OnStageMinVolt);      
   CreateProperty("Stage Low Voltage", "0", MM::Float, false, pAct, true);         

   pAct = new CPropertyAction (this, &DAZStage::OnStageMaxVolt);      
   CreateProperty("Stage High Voltage", "5", MM::Float, false, pAct, true);         

   pAct = new CPropertyAction (this, &DAZStage::OnStageMinPos); 
   CreateProperty(g_PropertyMinUm, "0", MM::Float, false, pAct, true); 

   pAct = new CPropertyAction (this, &DAZStage::OnStageMaxPos);      
   CreateProperty(g_PropertyMaxUm, "200", MM::Float, false, pAct, true);         
}  
 
DAZStage::~DAZStage()
{
}

void DAZStage::GetName(char* Name) const                                       
{                                                                            
   CDeviceUtils::CopyLimitedString(Name, g_DeviceNameDAZStage);                
}                                                                            
                                                                             
int DAZStage::Initialize() 
{
   // get list with available DA devices.  
   // TODO: this is a initialization parameter, which makes it harder for the end-user to set up!
   char deviceName[MM::MaxStrLength];
   availableDAs_.clear();
   unsigned int deviceIterator = 0;
   for(;;)
   {
      GetLoadedDeviceOfType(MM::SignalIODevice, deviceName, deviceIterator++);
      if( 0 < strlen(deviceName))
      {
         availableDAs_.push_back(std::string(deviceName));
      }
      else
         break;
   }



   CPropertyAction* pAct = new CPropertyAction (this, &DAZStage::OnDADevice);      
   std::string defaultDA = "Undefined";
   if (availableDAs_.size() >= 1)
      defaultDA = availableDAs_[0];
   CreateProperty("DA Device", defaultDA.c_str(), MM::String, false, pAct, false);         
   if (availableDAs_.size() >= 1)
      SetAllowedValues("DA Device", availableDAs_);
   else
      return ERR_NO_DA_DEVICE_FOUND;

   // This is needed, otherwise DeviceDA_ is not always set resulting in crashes
   // This could lead to strange problems if multiple DA devices are loaded
   SetProperty("DA Device", defaultDA.c_str());

   pAct = new CPropertyAction (this, &DAZStage::OnPosition);
   CreateProperty(MM::g_Keyword_Position, "0.0", MM::Float, false, pAct);
   double minPos = 0.0;
   int ret = GetProperty(g_PropertyMinUm, minPos);
   assert(ret == DEVICE_OK);
   double maxPos = 0.0;
   ret = GetProperty(g_PropertyMaxUm, maxPos);
   assert(ret == DEVICE_OK);
   SetPropertyLimits(MM::g_Keyword_Position, minPos, maxPos);

   ret = UpdateStatus();
   if (ret != DEVICE_OK)
      return ret;

   std::ostringstream tmp;
   tmp << DADevice_;
   LogMessage(tmp.str().c_str());

   if (DADevice_ != 0)
      DADevice_->GetLimits(minDAVolt_, maxDAVolt_);

   if (minStageVolt_ < minDAVolt_)
      return ERR_VOLT_OUT_OF_RANGE;

   originPos_ = minStagePos_;

   initialized_ = true;

   return DEVICE_OK;
}

int DAZStage::Shutdown()
{
   if (initialized_)
      initialized_ = false;

   return DEVICE_OK;
}

bool DAZStage::Busy()
{
   if (DADevice_ != 0)
      return DADevice_->Busy();

   // If we are here, there is a problem.  No way to report it.
   return false;
}

/*
 * Sets the position of the stage in um relative to the position of the origin
 */
int DAZStage::SetPositionUm(double pos)
{
   if (DADevice_ == 0)
      return ERR_NO_DA_DEVICE;

   double volt = ( (pos + originPos_) / (maxStagePos_ - minStagePos_)) * (maxStageVolt_ - minStageVolt_);
   if (volt > maxStageVolt_ || volt < minStageVolt_)
      return ERR_POS_OUT_OF_RANGE;

   pos_ = pos;
   return DADevice_->SetSignal(volt);
}

/*
 * Reports the current position of the stage in um relative to the origin
 */
int DAZStage::GetPositionUm(double& pos)
{
   if (DADevice_ == 0)
      return ERR_NO_DA_DEVICE;

   double volt;
   int ret = DADevice_->GetSignal(volt);
   if (ret != DEVICE_OK) 
      // DA Device cannot read, set position from cache
      pos = pos_;
   else
      pos = volt/(maxStageVolt_ - minStageVolt_) * (maxStagePos_ - minStagePos_) + originPos_;

   return DEVICE_OK;
}

/*
 * Sets a voltage (in mV) on the DA, relative to the minimum Stage position
 * The origin is NOT taken into account
 */
int DAZStage::SetPositionSteps(long steps)
{
   if (DADevice_ == 0)
      return ERR_NO_DA_DEVICE;

   // Interpret steps to be mV
   double volt = minStageVolt_  + (steps / 1000.0);
   if (volt < maxStageVolt_)
      DADevice_->SetSignal(volt);
   else
      return ERR_VOLT_OUT_OF_RANGE;

   pos_ = volt/(maxStageVolt_ - minStageVolt_) * (maxStagePos_ - minStagePos_) + originPos_;

   return DEVICE_OK;
}

int DAZStage::GetPositionSteps(long& steps)
{
   if (DADevice_ == 0)
      return ERR_NO_DA_DEVICE;

   double volt;
   int ret = DADevice_->GetSignal(volt);
   if (ret != DEVICE_OK)
      steps = (long) ((pos_ + originPos_)/(maxStagePos_ - minStagePos_) * (maxStageVolt_ - minStageVolt_) * 1000.0); 
   else
      steps = (long) ((volt - minStageVolt_) * 1000.0);

   return DEVICE_OK;
}

/*
 * Sets the origin (relative position 0) to the current absolute position
 */
int DAZStage::SetOrigin()
{
   if (DADevice_ == 0)
      return ERR_NO_DA_DEVICE;

   double volt;
   int ret = DADevice_->GetSignal(volt);
   if (ret != DEVICE_OK)
      return ret;

   // calculate absolute current position:
   originPos_ = volt/(maxStageVolt_ - minStageVolt_) * (maxStagePos_ - minStagePos_);

   if (originPos_ < minStagePos_ || originPos_ > maxStagePos_)
      return ERR_POS_OUT_OF_RANGE;

   return DEVICE_OK;
}

int DAZStage::GetLimits(double& min, double& max)
{
   min = minStagePos_;
   max = maxStagePos_;
   return DEVICE_OK;
}

int DAZStage::IsStageSequenceable(bool& isSequenceable) const 
{
   return DADevice_->IsDASequenceable(isSequenceable);
}

int DAZStage::GetStageSequenceMaxLength(long& nrEvents) const  
{
   return DADevice_->GetDASequenceMaxLength(nrEvents);
}

int DAZStage::StartStageSequence() const 
{
   return DADevice_->StartDASequence();
}

int DAZStage::StopStageSequence() const 
{
   return DADevice_->StopDASequence();
}

int DAZStage::ClearStageSequence() 
{
   return DADevice_->ClearDASequence();
}

int DAZStage::AddToStageSequence(double position) 
{
   double voltage;

      voltage = ( (position + originPos_) / (maxStagePos_ - minStagePos_)) * 
                     (maxStageVolt_ - minStageVolt_);
      if (voltage > maxStageVolt_)
         voltage = maxStageVolt_;
      else if (voltage < minStageVolt_)
         voltage = minStageVolt_;
   
   return DADevice_->AddToDASequence(voltage);
}

int DAZStage::SendStageSequence() const
{
   return DADevice_->SendDASequence();
}


///////////////////////////////////////
// Action Interface
//////////////////////////////////////
int DAZStage::OnDADevice(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      pProp->Set(DADeviceName_.c_str());
   }
   else if (eAct == MM::AfterSet)
   {
      std::string DADeviceName;
      pProp->Get(DADeviceName);
      MM::SignalIO* DADevice = (MM::SignalIO*) GetDevice(DADeviceName.c_str());
      if (DADevice != 0) {
         DADevice_ = DADevice;
         DADeviceName_ = DADeviceName;
      } else
         return ERR_INVALID_DEVICE_NAME;
      if (initialized_)
         DADevice_->GetLimits(minDAVolt_, maxDAVolt_);
   }
   return DEVICE_OK;
}
int DAZStage::OnPosition(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      double pos;
      int ret = GetPositionUm(pos);
      if (ret != DEVICE_OK)
         return ret;
      pProp->Set(pos);
   }
   else if (eAct == MM::AfterSet)
   {
      double pos;
      pProp->Get(pos);
      return SetPositionUm(pos);
   }
   return DEVICE_OK;
}
int DAZStage::OnStageMinVolt(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      pProp->Set(minStageVolt_);
   }
   else if (eAct == MM::AfterSet)
   {
      double minStageVolt;
      pProp->Get(minStageVolt);
      if (minStageVolt >= minDAVolt_ && minStageVolt < maxDAVolt_)
         minStageVolt_ = minStageVolt;
      else
         return ERR_VOLT_OUT_OF_RANGE;
   }
   return DEVICE_OK;
}

int DAZStage::OnStageMaxVolt(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      pProp->Set(maxStageVolt_);
   }
   else if (eAct == MM::AfterSet)
   {
      double maxStageVolt;
      pProp->Get(maxStageVolt);
      if (maxStageVolt > minDAVolt_ && maxStageVolt <= maxDAVolt_)
         maxStageVolt_ = maxStageVolt;
      else
         return ERR_VOLT_OUT_OF_RANGE;
   }
   return DEVICE_OK;
}

int DAZStage::OnStageMinPos(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      pProp->Set(minStagePos_);
   }
   else if (eAct == MM::AfterSet)
   {
      pProp->Get(minStagePos_);
   }
   return DEVICE_OK;
}

int DAZStage::OnStageMaxPos(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      pProp->Set(maxStagePos_);
   }
   else if (eAct == MM::AfterSet)
   {
      pProp->Get(maxStagePos_);
   }
   return DEVICE_OK;
}

/**************************
 * DAXYStage implementation
 */

DAXYStage::DAXYStage() :
   DADeviceNameX_ (""),
   DADeviceNameY_ (""),
   initialized_ (false),
   minDAVoltX_ (0.0),
   maxDAVoltX_ (10.0),
   minDAVoltY_ (0.0),
   maxDAVoltY_ (10.0),
   minStageVoltX_ (0.0),
   maxStageVoltX_ (5.0),
   minStageVoltY_ (0.0),
   maxStageVoltY_ (5.0),
   minStagePosX_ (0.0),
   maxStagePosX_ (200.0),
   minStagePosY_ (0.0),
   maxStagePosY_ (200.0),
   posX_ (0.0),
   posY_ (0.0),
   originPosX_ (0.0),
   originPosY_ (0.0),
   stepSizeXUm_(1),
   stepSizeYUm_(1)
{
   InitializeDefaultErrorMessages();

   SetErrorText(ERR_INVALID_DEVICE_NAME, "Please select a valid DA device");
   SetErrorText(ERR_NO_DA_DEVICE, "No DA Device selected");
   SetErrorText(ERR_VOLT_OUT_OF_RANGE, "The DA Device cannot set the requested voltage");
   SetErrorText(ERR_POS_OUT_OF_RANGE, "The requested position is out of range");
   SetErrorText(ERR_NO_DA_DEVICE_FOUND, "No DA Device loaded");

   // Name                                                                   
   CreateProperty(MM::g_Keyword_Name, g_DeviceNameDAZStage, MM::String, true); 
                                                                             
   // Description                                                            
   CreateProperty(MM::g_Keyword_Description, "XYStage controlled with voltage provided by a Digital to Analog output", MM::String, true);

   // min volts
   CPropertyAction* pAct = new CPropertyAction (this, &DAXYStage::OnStageMinVoltX);      
   CreateProperty("Stage X Low Voltage", "0", MM::Float, false, pAct, true);         

   pAct = new CPropertyAction (this, &DAXYStage::OnStageMinVoltY);      
   CreateProperty("Stage Y Low Voltage", "0", MM::Float, false, pAct, true);    
   //max volts
   pAct = new CPropertyAction (this, &DAXYStage::OnStageMaxVoltX);      
   CreateProperty("Stage X High Voltage", "5", MM::Float, false, pAct, true);        

   pAct = new CPropertyAction (this, &DAXYStage::OnStageMaxVoltY);      
   CreateProperty("Stage Y High Voltage", "5", MM::Float, false, pAct, true);   
   // min pos
   /*
   pAct = new CPropertyAction (this, &DAXYStage::OnStageMinPosX); 
   CreateProperty(g_PropertyMinUm, "0", MM::Float, false, pAct, true); 
   
   pAct = new CPropertyAction (this, &DAXYStage::OnStageMinPosX); 
   CreateProperty(g_PropertyMinUm, "0", MM::Float, false, pAct, true); 
   //max pos
   pAct = new CPropertyAction (this, &DAXYStage::OnStageMaxPosX);      
   CreateProperty(g_PropertyMaxUm, "200", MM::Float, false, pAct, true);
   
   pAct = new CPropertyAction (this, &DAXYStage::OnStageMaxPosX);      
   CreateProperty(g_PropertyMaxUm, "200", MM::Float, false, pAct, true);  
   */
}  
 
DAXYStage::~DAXYStage()
{
}

void DAXYStage::GetName(char* Name) const                                       
{                                                                            
   CDeviceUtils::CopyLimitedString(Name, g_DeviceNameDAXYStage);                
}                                                                            
                                                                             
int DAXYStage::Initialize() 
{
   // get list with available DA devices.  
   // TODO: this is a initialization parameter, which makes it harder for the end-user to set up!
   char deviceName[MM::MaxStrLength];
   availableDAs_.clear();
   unsigned int deviceIterator = 0;
   for(;;)
   {
      GetLoadedDeviceOfType(MM::SignalIODevice, deviceName, deviceIterator++);
      if( 0 < strlen(deviceName))
      {
         availableDAs_.push_back(std::string(deviceName));
      }
      else
         break;
   }



   CPropertyAction* pAct = new CPropertyAction (this, &DAXYStage::OnDADeviceX);      
   std::string defaultXDA = "Undefined";
   if (availableDAs_.size() >= 1)
      defaultXDA = availableDAs_[0];
   CreateProperty("X DA Device", defaultXDA.c_str(), MM::String, false, pAct, false);         
   if (availableDAs_.size() >= 1)
      SetAllowedValues("X DA Device", availableDAs_);
   else
      return ERR_NO_DA_DEVICE_FOUND;

   pAct = new CPropertyAction (this, &DAXYStage::OnDADeviceY);      
   std::string defaultYDA = "Undefined";
   if (availableDAs_.size() >= 1)
      defaultYDA = availableDAs_[1];
   CreateProperty("Y DA Device", defaultYDA.c_str(), MM::String, false, pAct, false);         
   if (availableDAs_.size() >= 1)
      SetAllowedValues("Y DA Device", availableDAs_);
   else
      return ERR_NO_DA_DEVICE_FOUND;


   // This is needed, otherwise DeviceDA_ is not always set resulting in crashes 
   // This could lead to strange problems if multiple DA devices are loaded
   SetProperty("X DA Device", defaultXDA.c_str());
   SetProperty("Y DA Device", defaultYDA.c_str());
   /*
   pAct = new CPropertyAction (this, &DAXYStage::OnPosition);
   CreateProperty(MM::g_Keyword_Position, "0.0", MM::Float, false, pAct);
   double minPos = 0.0;
   int ret = GetProperty(g_PropertyMinUm, minPos);
   assert(ret == DEVICE_OK);
   double maxPos = 0.0;
   ret = GetProperty(g_PropertyMaxUm, maxPos);
   assert(ret == DEVICE_OK);
   SetPropertyLimits(MM::g_Keyword_Position, minPos, maxPos);
   */
   int ret = UpdateStatus();
   if (ret != DEVICE_OK)
      return ret;

   std::ostringstream tmp;
   tmp << DADeviceX_;
   LogMessage(tmp.str().c_str());

   if (DADeviceX_ != 0)
      DADeviceX_->GetLimits(minDAVoltX_, maxDAVoltX_);

   if (DADeviceY_ != 0)
      DADeviceY_->GetLimits(minDAVoltY_, maxDAVoltY_);

   if (minStageVoltX_ < minDAVoltX_)
      return ERR_VOLT_OUT_OF_RANGE;

   if (minStageVoltY_ < minDAVoltY_)
      return ERR_VOLT_OUT_OF_RANGE;

   originPosX_ = minStagePosX_;
   originPosY_ = minStagePosY_;


   initialized_ = true;

   return DEVICE_OK;
}

int DAXYStage::Shutdown()
{
   if (initialized_)
      initialized_ = false;

   return DEVICE_OK;
}

bool DAXYStage::Busy()
{
   if ((DADeviceX_ != 0) && (DADeviceY_ != 0))
	   return DADeviceX_->Busy() || DADeviceY_->Busy();

   // If we are here, there is a problem.  No way to report it.
   return false;
}

int DAXYStage::Home()
{
   return DEVICE_OK;

};


int DAXYStage::Stop()
{
   return DEVICE_OK;

};

/*
 * Sets a voltage (in mV) on the DA, relative to the minimum Stage position
 * The origin is NOT taken into account
 */
int DAXYStage::SetPositionSteps(long stepsX, long stepsY)
{
   if (DADeviceX_ == 0 || DADeviceY_ == 0 )
      return ERR_NO_DA_DEVICE;

   // Interpret steps to be mV
   double voltX = minStageVoltX_  + (stepsX / 1000.0);
   if (voltX > minStageVoltX_&& voltX < maxStageVoltX_  )
      DADeviceX_->SetSignal(voltX);
   else
      return ERR_VOLT_OUT_OF_RANGE;

   double voltY = minStageVoltY_  + (stepsY / 1000.0);
   if (voltY < maxStageVoltY_)
      DADeviceY_->SetSignal(voltY);
   else
      return ERR_VOLT_OUT_OF_RANGE;

   posX_ = voltX/(maxStageVoltX_ - minStageVoltX_) * (maxStagePosX_ - minStagePosX_) + originPosX_;
   posY_ = voltY/(maxStageVoltY_ - minStageVoltY_) * (maxStagePosY_ - minStagePosY_) + originPosY_;

   return DEVICE_OK;
}

int DAXYStage::GetPositionSteps(long& stepsX, long& stepsY)
{
   if (DADeviceX_ == 0 || DADeviceY_ == 0 )
      return ERR_NO_DA_DEVICE;

   double voltX = 0, voltY = 0;
   int ret = DADeviceX_->GetSignal(voltX);
   if (ret != DEVICE_OK) {
      stepsX = (long) ((posX_ + originPosX_)/(maxStagePosX_ - minStagePosX_) * (maxStageVoltX_ - minStageVoltX_) * 1000.0); 
   } else {
      stepsX = (long) ((voltX - minStageVoltX_) * 1000.0);
   }
   ret = DADeviceY_->GetSignal(voltY);
   if (ret != DEVICE_OK) {
      stepsY = (long) ((posY_ + originPosY_)/(maxStagePosY_ - minStagePosY_) * (maxStageVoltY_ - minStageVoltY_) * 1000.0); 
   }
   else{
      stepsY = (long) ((voltY - minStageVoltY_) * 1000.0);
   }
   return DEVICE_OK;
}

int DAXYStage::SetRelativePositionSteps(long x, long y)                                                           
   {                                                                                                      
      long xSteps, ySteps;                                                                                
      GetPositionSteps(xSteps, ySteps);                                                   

      return this->SetPositionSteps(xSteps+x, ySteps+y);                                                  
   }

int DAXYStage::SetPositionUm(double x, double y)
{
   if (DADeviceX_ == 0 || DADeviceY_ == 0 )
      return ERR_NO_DA_DEVICE;

   double voltX = ( (x + originPosX_) / (maxStagePosX_ - minStagePosX_)) * (maxStageVoltX_ - minStageVoltX_);
   if (voltX > maxStageVoltX_ || voltX < minStageVoltX_)
      return ERR_POS_OUT_OF_RANGE;
   double voltY = ( (y + originPosY_) / (maxStagePosY_ - minStagePosY_)) * (maxStageVoltY_ - minStageVoltY_);
   if (voltY > maxStageVoltY_ || voltY < minStageVoltY_)
      return ERR_POS_OUT_OF_RANGE;
   
   //posY_ = y;


   int ret = DADeviceX_->SetSignal(voltX);
   if(ret != DEVICE_OK) return ret;
   ret = DADeviceY_->SetSignal(voltY);
   if(ret != DEVICE_OK) return ret;

   return ret;
}

int DAXYStage::GetPositionUm(double& x, double& y)
{
   if (DADeviceX_ == 0 || DADeviceY_ == 0 )
      return ERR_NO_DA_DEVICE;

   double voltX, voltY;
   int ret = DADeviceX_->GetSignal(voltX);
   if (ret != DEVICE_OK) 
      // DA Device cannot read, set position from cache
      x = posX_;
   else
      x = voltX/(maxStageVoltX_ - minStageVoltX_) * (maxStagePosX_ - minStagePosX_) + originPosX_;

   ret = DADeviceY_->GetSignal(voltY);
   if (ret != DEVICE_OK) 
      // DA Device cannot read, set position from cache
      y = posY_;
   else
      y = voltY/(maxStageVoltY_ - minStageVoltY_) * (maxStagePosY_ - minStagePosY_) + originPosY_;

   return DEVICE_OK;
}


/*
 * Sets the origin (relative position 0) to the current absolute position
 */
int DAXYStage::SetOrigin()
{
   if (DADeviceX_ == 0 || DADeviceY_ == 0 )
      return ERR_NO_DA_DEVICE;

   double voltX, voltY;
   int ret = DADeviceX_->GetSignal(voltX);
   if (ret != DEVICE_OK)
      return ret;
   ret = DADeviceY_->GetSignal(voltY);
   if (ret != DEVICE_OK)
      return ret;

   // calculate absolute current position:
   originPosX_ = voltX/(maxStageVoltX_ - minStageVoltX_) * (maxStagePosX_ - minStagePosX_);

   if (originPosX_ < minStagePosX_ || originPosX_ > maxStagePosX_)
      return ERR_POS_OUT_OF_RANGE;

   return DEVICE_OK;
}

int DAXYStage::GetLimitsUm(double& /*xMin*/, double& /*xMax*/, double& /*yMin*/, double& /*yMax*/)
{
	return DEVICE_UNSUPPORTED_COMMAND;
}

int DAXYStage::GetStepLimits(long& /*xMin*/, long& /*xMax*/, long& /*yMin*/, long& /*yMax*/)
{
   return DEVICE_UNSUPPORTED_COMMAND;
}

int DAXYStage::IsXYStageSequenceable(bool& isSequenceable) const
{
	bool x, y;
   DADeviceX_->IsDASequenceable(x);
	DADeviceY_->IsDASequenceable(y);
	isSequenceable = x && y;
	return DEVICE_OK ;
}

int DAXYStage::GetXYStageSequenceMaxLength(long& nrEvents) const  
{
	long x, y;
	int ret = DADeviceX_->GetDASequenceMaxLength(x);
	if(ret !=DEVICE_OK) return ret;
	ret = DADeviceY_->GetDASequenceMaxLength(y);
	if(ret !=DEVICE_OK) return ret;
	nrEvents = (std::min)(x,y);
    return ret;
}

int DAXYStage::StartXYStageSequence() const 
{
	int ret = DADeviceX_->StartDASequence();
	if(ret !=DEVICE_OK) return ret;
	ret = DADeviceY_->StartDASequence();
	if(ret !=DEVICE_OK) return ret;
   return ret;
}

int DAXYStage::StopXYStageSequence() const 
{
   int ret = DADeviceX_->StopDASequence();
	if(ret !=DEVICE_OK) return ret;
	ret = DADeviceY_->StopDASequence();
	if(ret !=DEVICE_OK) return ret;
   return DEVICE_OK;
}

int DAXYStage::ClearXYStageSequence() 
{
    int ret = DADeviceX_->ClearDASequence();
	if(ret !=DEVICE_OK) return ret;
	ret = DADeviceY_->ClearDASequence();
	if(ret !=DEVICE_OK) return ret;
   return DEVICE_OK;
}

int DAXYStage::AddToXYStageSequence(double positionX, double positionY) 
{
   double voltageX, voltageY;

      voltageX = ( (positionX + originPosX_) / (maxStagePosX_ - minStagePosX_)) * 
                     (maxStageVoltX_ - minStageVoltX_);
      if (voltageX > maxStageVoltX_)
         voltageX = maxStageVoltX_;
      else if (voltageX < minStageVoltX_)
         voltageX = minStageVoltX_;

      voltageY = ( (positionY + originPosY_) / (maxStagePosY_ - minStagePosY_)) * 
                     (maxStageVoltY_ - minStageVoltY_);
      if (voltageY > maxStageVoltY_)
         voltageY = maxStageVoltY_;
      else if (voltageY < minStageVoltY_)
         voltageY = minStageVoltY_;
   
   int ret = DADeviceX_->AddToDASequence(voltageX);
   if(ret != DEVICE_OK) return ret;

   ret = DADeviceY_->AddToDASequence(voltageY);
   if(ret != DEVICE_OK) return ret;

   return DEVICE_OK;
}

int DAXYStage::SendXYStageSequence() const
{
   int ret = DADeviceX_->SendDASequence();
   if(ret != DEVICE_OK) return ret;
   ret = DADeviceY_->SendDASequence();
   return ret;
}



///////////////////////////////////////
// Action Interface
//////////////////////////////////////
int DAXYStage::OnDADeviceX(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      pProp->Set(DADeviceNameX_.c_str());
   }
   else if (eAct == MM::AfterSet)
   {
      std::string DADeviceName;
      pProp->Get(DADeviceName);
      MM::SignalIO* DADevice = (MM::SignalIO*) GetDevice(DADeviceName.c_str());
      if (DADevice != 0) {
         DADeviceX_ = DADevice;
         DADeviceNameX_ = DADeviceName;
      } else
         return ERR_INVALID_DEVICE_NAME;
      if (initialized_)
         DADeviceX_->GetLimits(minDAVoltX_, maxDAVoltX_);
   }
   return DEVICE_OK;
}

int DAXYStage::OnDADeviceY(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      pProp->Set(DADeviceNameY_.c_str());
   }
   else if (eAct == MM::AfterSet)
   {
      std::string DADeviceName;
      pProp->Get(DADeviceName);
      MM::SignalIO* DADevice = (MM::SignalIO*) GetDevice(DADeviceName.c_str());
      if (DADevice != 0) {
         DADeviceY_ = DADevice;
         DADeviceNameY_ = DADeviceName;
      } else
         return ERR_INVALID_DEVICE_NAME;
      if (initialized_)
         DADeviceY_->GetLimits(minDAVoltY_, maxDAVoltY_);
   }
   return DEVICE_OK;
}



int DAXYStage::OnStageMinVoltX(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      pProp->Set(minStageVoltX_);
   }
   else if (eAct == MM::AfterSet)
   {
      double minStageVolt;
      pProp->Get(minStageVolt);
      if (minStageVolt >= minDAVoltX_ && minStageVolt < maxDAVoltX_)
         minStageVoltX_ = minStageVolt;
      else
         return ERR_VOLT_OUT_OF_RANGE;
   }
   return DEVICE_OK;
}

int DAXYStage::OnStageMaxVoltX(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      pProp->Set(maxStageVoltX_);
   }
   else if (eAct == MM::AfterSet)
   {
      double maxStageVolt;
      pProp->Get(maxStageVolt);
      if (maxStageVolt > minDAVoltX_ && maxStageVolt <= maxDAVoltX_)
         maxStageVoltX_ = maxStageVolt;
      else
         return ERR_VOLT_OUT_OF_RANGE;
   }
   return DEVICE_OK;
}

int DAXYStage::OnStageMinVoltY(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      pProp->Set(minStageVoltY_);
   }
   else if (eAct == MM::AfterSet)
   {
      double minStageVolt;
      pProp->Get(minStageVolt);
      if (minStageVolt >= minDAVoltY_ && minStageVolt < maxDAVoltY_)
         minStageVoltY_ = minStageVolt;
      else
         return ERR_VOLT_OUT_OF_RANGE;
   }
   return DEVICE_OK;
}

int DAXYStage::OnStageMaxVoltY(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      pProp->Set(maxStageVoltY_);
   }
   else if (eAct == MM::AfterSet)
   {
      double maxStageVolt;
      pProp->Get(maxStageVolt);
      if (maxStageVolt > minDAVoltY_ && maxStageVolt <= maxDAVoltY_)
         maxStageVoltY_ = maxStageVolt;
      else
         return ERR_VOLT_OUT_OF_RANGE;
   }
   return DEVICE_OK;
}



/**************************
 * AutoFocusStage implementation
 */

AutoFocusStage::AutoFocusStage() :
   AutoFocusDeviceName_ (""),
   initialized_ (false),
   pos_ (0.0),
   originPos_ (0.0)
{
   InitializeDefaultErrorMessages();

   SetErrorText(ERR_INVALID_DEVICE_NAME, "Please select a valid AutoFocus device");
   SetErrorText(ERR_NO_AUTOFOCUS_DEVICE, "No AutoFocus Device selected");
   SetErrorText(ERR_NO_AUTOFOCUS_DEVICE_FOUND, "No AutoFocus Device loaded");
   SetErrorText(ERR_DEFINITE_FOCUS_TIMEOUT, "Definite Focus timed out.  Increase the value of Core-Timeout if the definite focus is still searching");

   // Name                                                                   
   CreateProperty(MM::g_Keyword_Name, g_DeviceNameAutoFocusStage, MM::String, true); 
                                                                             
   // Description                                                            
   CreateProperty(MM::g_Keyword_Description, "AutoFocus offset treated as a ZStage", MM::String, true);

}  
 
AutoFocusStage::~AutoFocusStage()
{
}

void AutoFocusStage::GetName(char* Name) const                                       
{                                                                            
   CDeviceUtils::CopyLimitedString(Name, g_DeviceNameAutoFocusStage);                
}                                                                            
                                                                             
int AutoFocusStage::Initialize() 
{
   // get list with available AutoFocus devices.
   // TODO: this is a initialization parameter, which makes it harder for the end-user to set up!
   char deviceName[MM::MaxStrLength];
   unsigned int deviceIterator = 0;
   for(;;)
   {
      GetLoadedDeviceOfType(MM::AutoFocusDevice, deviceName, deviceIterator++);
      if( 0 < strlen(deviceName))
      {
         availableAutoFocusDevices_.push_back(std::string(deviceName));
      }
      else
         break;
   }




   CPropertyAction* pAct = new CPropertyAction (this, &AutoFocusStage::OnAutoFocusDevice);      
   std::string defaultAutoFocus = "Undefined";
   if (availableAutoFocusDevices_.size() >= 1)
      defaultAutoFocus = availableAutoFocusDevices_[0];
   CreateProperty("AutoFocus Device", defaultAutoFocus.c_str(), MM::String, false, pAct, false);         
   if (availableAutoFocusDevices_.size() >= 1)
      SetAllowedValues("AutoFocus Device", availableAutoFocusDevices_);
   else
      return ERR_NO_AUTOFOCUS_DEVICE_FOUND;

   // This is needed, otherwise DeviceAUtofocus_ is not always set resulting in crashes
   // This could lead to strange problems if multiple AutoFocus devices are loaded
   SetProperty("AutoFocus Device", defaultAutoFocus.c_str());

   int ret = UpdateStatus();
   if (ret != DEVICE_OK)
      return ret;

   std::ostringstream tmp;
   tmp << AutoFocusDevice_;
   LogMessage(tmp.str().c_str());

   initialized_ = true;

   return DEVICE_OK;
}

int AutoFocusStage::Shutdown()
{
   if (initialized_)
      initialized_ = false;

   return DEVICE_OK;
}

bool AutoFocusStage::Busy()
{
   if (AutoFocusDevice_ != 0)
      return AutoFocusDevice_->Busy();

   // If we are here, there is a problem.  No way to report it.
   return false;
}

/*
 * Sets the position of the stage in um relative to the position of the origin
 */
int AutoFocusStage::SetPositionUm(double pos)
{
   if (AutoFocusDevice_ == 0)
      return ERR_NO_AUTOFOCUS_DEVICE;

   return AutoFocusDevice_->SetOffset(pos);
}

/*
 * Reports the current position of the stage in um relative to the origin
 */
int AutoFocusStage::GetPositionUm(double& pos)
{
   if (AutoFocusDevice_ == 0)
      return ERR_NO_AUTOFOCUS_DEVICE;

   return  AutoFocusDevice_->GetOffset(pos);;
}

/*
 * Sets a voltage (in mV) on the DA, relative to the minimum Stage position
 * The origin is NOT taken into account
 */
int AutoFocusStage::SetPositionSteps(long /* steps */)
{
   if (AutoFocusDevice_ == 0)
      return ERR_NO_AUTOFOCUS_DEVICE;

   return  DEVICE_UNSUPPORTED_COMMAND;
}

int AutoFocusStage::GetPositionSteps(long& /*steps */)
{
   if (AutoFocusDevice_ == 0)
      return ERR_NO_AUTOFOCUS_DEVICE;

   return  DEVICE_UNSUPPORTED_COMMAND;
}

/*
 * Sets the origin (relative position 0) to the current absolute position
 */
int AutoFocusStage::SetOrigin()
{
   if (AutoFocusDevice_ == 0)
      return ERR_NO_AUTOFOCUS_DEVICE;

   return  DEVICE_UNSUPPORTED_COMMAND;
}

int AutoFocusStage::GetLimits(double& /*min*/, double& /*max*/)
{
   if (AutoFocusDevice_ == 0)
      return ERR_NO_AUTOFOCUS_DEVICE;

   return  DEVICE_UNSUPPORTED_COMMAND;
}


///////////////////////////////////////
// Action Interface
//////////////////////////////////////
int AutoFocusStage::OnAutoFocusDevice(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      pProp->Set(AutoFocusDeviceName_.c_str());
   }
   else if (eAct == MM::AfterSet)
   {
      std::string AutoFocusDeviceName;
      pProp->Get(AutoFocusDeviceName);
      MM::AutoFocus* AutoFocusDevice = (MM::AutoFocus*) GetDevice(AutoFocusDeviceName.c_str());
      if (AutoFocusDevice != 0) {
         AutoFocusDevice_ = AutoFocusDevice;
         AutoFocusDeviceName_ = AutoFocusDeviceName;
      } else
         return ERR_INVALID_DEVICE_NAME;
   }
   return DEVICE_OK;
}



/**********************************************************************
 * StateDeviceShutter implementation
 */
StateDeviceShutter::StateDeviceShutter() :
   stateDeviceName_ (""),
   stateDevice_ (0),
   initialized_ (false)
{
   InitializeDefaultErrorMessages();

   SetErrorText(ERR_INVALID_DEVICE_NAME, "Please select a valid State device");
   SetErrorText(ERR_NO_STATE_DEVICE, "No State Device selected");
   SetErrorText(ERR_NO_STATE_DEVICE_FOUND, "No State Device loaded");
   SetErrorText(ERR_TIMEOUT, "Device was busy.  Try increasing the Core-Timeout property");

   // Name                                                                   
   CreateProperty(MM::g_Keyword_Name, g_DeviceNameStateDeviceShutter, MM::String, true); 
                                                                             
   // Description                                                            
   CreateProperty(MM::g_Keyword_Description, "State device that is used as a shutter", MM::String, true);

}  
 
StateDeviceShutter::~StateDeviceShutter()
{
   Shutdown();
}

void StateDeviceShutter::GetName(char* Name) const
{
   CDeviceUtils::CopyLimitedString(Name, g_DeviceNameStateDeviceShutter);
}                                                                            
                                                                             
int StateDeviceShutter::Initialize() 
{
   // get list with available DA devices. 
   char deviceName[MM::MaxStrLength];
   unsigned int deviceIterator = 0;
   for(;;)
   {
      GetLoadedDeviceOfType(MM::StateDevice, deviceName, deviceIterator++);
      if( 0 < strlen(deviceName))
      {
         availableStateDevices_.push_back(std::string(deviceName));
      }
      else
         break;
   }





   std::vector<std::string>::iterator it;
   it = availableStateDevices_.begin();
   availableStateDevices_.insert(it, g_NoDevice);


   CPropertyAction* pAct = new CPropertyAction (this, &StateDeviceShutter::OnStateDevice);      
   std::string defaultStateDevice = g_NoDevice;
   CreateProperty("State Device", defaultStateDevice.c_str(), MM::String, false, pAct, false);         
   if (availableStateDevices_.size() >= 1)
      SetAllowedValues("State Device", availableStateDevices_);
   else
      return ERR_NO_STATE_DEVICE_FOUND;

   SetProperty("State Device", defaultStateDevice.c_str());

   initialized_ = true;

   return DEVICE_OK;
}

bool StateDeviceShutter::Busy()
{
   if (stateDevice_ != 0)
      return stateDevice_->Busy();

   // If we are here, there is a problem.  No way to report it.
   return false;
}

/*
 * Opens or closes the shutter. 
 */
int StateDeviceShutter::SetOpen(bool open)
{
   if (stateDevice_ == 0)
      return DEVICE_OK;

   int ret = WaitWhileBusy();
   if (ret != DEVICE_OK)
      return ret;

   return stateDevice_->SetGateOpen(open);
}

int StateDeviceShutter::GetOpen(bool& open)
{
   if (stateDevice_ == 0)
      return DEVICE_OK;

   int ret = WaitWhileBusy();
   if (ret != DEVICE_OK)
      return ret;

   return stateDevice_->GetGateOpen(open);
}

int StateDeviceShutter::WaitWhileBusy()
{
   if (stateDevice_ == 0)
      return DEVICE_OK;

   bool busy = true;
   char timeout[MM::MaxStrLength];
   GetCoreCallback()->GetDeviceProperty("Core", "TimeoutMs", timeout);
   MM::MMTime dTimeout = MM::MMTime (atof(timeout) * 1000.0);
   MM::MMTime start = GetCoreCallback()->GetCurrentMMTime();
   while (busy && (GetCoreCallback()->GetCurrentMMTime() - start) < dTimeout)
      busy = Busy();

   if (busy)
      return ERR_TIMEOUT;

   return DEVICE_OK;
}

///////////////////////////////////////
// Action Interface
//////////////////////////////////////
int StateDeviceShutter::OnStateDevice(MM::PropertyBase* pProp, MM::ActionType eAct)
{
   if (eAct == MM::BeforeGet)
   {
      pProp->Set(stateDeviceName_.c_str());
   }
   else if (eAct == MM::AfterSet)
  {
      // Avoid leaving a State device in the closed positions!
      SetOpen(true);
      
      std::string stateDeviceName;
      pProp->Get(stateDeviceName);
      if (stateDeviceName == g_NoDevice) {
         stateDevice_ = 0;
         stateDeviceName_ = g_NoDevice;
      } else {
         MM::State* stateDevice = (MM::State*) GetDevice(stateDeviceName.c_str());
         if (stateDevice != 0) {
            stateDevice_ = stateDevice;
            stateDeviceName_ = stateDeviceName;
         } else {
            return ERR_INVALID_DEVICE_NAME;
         }
      }
   }
   return DEVICE_OK;
}


