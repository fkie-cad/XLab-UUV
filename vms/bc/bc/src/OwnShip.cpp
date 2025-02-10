/*   Bridge Command 5.0 Ship Simulator
     Copyright (C) 2014 James Packer

     This program is free software; you can redistribute it and/or modify
     it under the terms of the GNU General Public License version 2 as
     published by the Free Software Foundation

     This program is distributed in the hope that it will be useful,
     but WITHOUT ANY WARRANTY; without even the implied warranty of
     MERCHANTABILITY Or FITNESS For A PARTICULAR PURPOSE.  See the
     GNU General Public License For more details.

     You should have received a copy of the GNU General Public License along
     with this program; if not, write to the Free Software Foundation, Inc.,
     51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA. */

// Extends from the general 'Ship' class
#include "OwnShip.hpp"

#include <algorithm>
#include <cstdlib>  //For rand()

#include "Angles.hpp"
#include "Constants.hpp"
#include "IniFile.hpp"
#include "ScenarioDataStructure.hpp"
#include "SimulationModel.hpp"
#include "Terrain.hpp"
#include "Utilities.hpp"

// using namespace irr;

void OwnShip::load(OwnShipData ownShipData,
                   irr::core::vector3di numberOfContactPoints,
                   irr::scene::ISceneManager* smgr, SimulationModel* model,
                   Terrain* terrain, irr::IrrlichtDevice* dev) {
  // Store reference to terrain
  this->terrain = terrain;

  // Store reference to model
  this->model = model;

  // reference to device (for logging etc)
  device = dev;

  // Load from ownShip.ini file
  std::string ownShipName = ownShipData.ownShipName;
  // Get initial position and heading, and set these
  axialSpd = ownShipData.initialSpeed * KTS_TO_MPS;
  lateralSpd = 0;
  xPos = model->longToX(ownShipData.initialLong);
  yPos = 0;
  zPos = model->latToZ(ownShipData.initialLat);
  hdg = ownShipData.initialBearing;  // DEE_DEC22  this is initial heading

  basePath = "Models/Ownship/" + ownShipName + "/";
  std::string userFolder = Utilities::getUserDir();
  // Read model from user dir if it exists there.
  if (Utilities::pathExists(userFolder + basePath)) {
    basePath = userFolder + basePath;
  }

  // Load from boat.ini file if it exists
  std::string shipIniFilename = basePath + "boat.ini";

  // Construct the radar config file name, to be used later by the radar
  radarConfigFile = basePath + "radar.ini";

  // get the model file
  std::string ownShipFileName =
      IniFile::iniFileToString(shipIniFilename, "FileName");
  std::string ownShipFullPath = basePath + ownShipFileName;

  // Load dynamics settings
  //  DEE_DEC22 ship parameters directly from ini file

  // DEE_DEC22 if block coefficient is defined then shipMass and Izz will be
  // overridden and calculated

  shipMass = IniFile::iniFileTof32(
      shipIniFilename,
      "Mass");  // DEE_DEC22 supersede this and replace it with displacement
                // calculate mass from Cb * L * B * draught
                // Cb block coefficient typically 0.87
  Izz = IniFile::iniFileTof32(
      shipIniFilename,
      "Inertia");  // DEE_DEC22 supersede this with a calculated Izz
                   // if not defined or if zero in the ini file

  maxEngineRevs = IniFile::iniFileTof32(shipIniFilename, "MaxRevs");
  dynamicsSpeedA = IniFile::iniFileTof32(shipIniFilename, "DynamicsSpeedA");
  dynamicsSpeedB = IniFile::iniFileTof32(shipIniFilename, "DynamicsSpeedB");
  dynamicsTurnDragA =
      IniFile::iniFileTof32(shipIniFilename, "DynamicsTurnDragA");
  dynamicsTurnDragB =
      IniFile::iniFileTof32(shipIniFilename, "DynamicsTurnDragB");
  rudderA = IniFile::iniFileTof32(shipIniFilename, "RudderA");
  rudderB = IniFile::iniFileTof32(shipIniFilename, "RudderB");
  rudderBAstern = IniFile::iniFileTof32(shipIniFilename, "RudderBAstern");
  maxForce = IniFile::iniFileTof32(shipIniFilename, "Max_propulsion_force");
  propellorSpacing = IniFile::iniFileTof32(shipIniFilename, "PropSpace");
  asternEfficiency = IniFile::iniFileTof32(
      shipIniFilename, "AsternEfficiency");  // (Optional, default 1)
  propWalkAhead = IniFile::iniFileTof32(
      shipIniFilename, "PropWalkAhead");  // (Optional, default 0)
  propWalkAstern = IniFile::iniFileTof32(
      shipIniFilename, "PropWalkAstern");  // (Optional, default 0)

  // Pitch and roll parameters: FIXME for hardcoding, and in future should be
  // linked to the water's movements

  // DEE todo parametarise this to a function of the GM and hence GZ and inertia
  // about a longitudinal axis Iyy, or at least a good approximation of it.
  // Future modelling could also try to model parametric rolling.
  rollPeriod = IniFile::iniFileTof32(
      shipIniFilename,
      "RollPeriod");  // Softcoded roll period Tr a function of the ships
                      // condition indpendant of Te, the wave encounter period
  // DEE_DEC22 should add a pitch period also something like pitchPeriod =
  // rollPeriod * (Ixx / Iyy);
  rudderMaxSpeed = IniFile::iniFileTof32(
      shipIniFilename,
      "RudderAngularVelocity");  // Softcoded angular speed of the steering gear
                                 // DEE ^^^^^
                                 // DEE_NOV22 vvvv
  // read from boat.ini the pararmeters DEE has added with respect to modelling
  // the azimuth drives
  azimuthDriveEngineIdleRPM = IniFile::iniFileTof32(
      shipIniFilename,
      "azimuthDriveEngineIdleRPM");  // the engine revs at which each engine
                                     // will idle at
  azimuthDriveClutchEngageRPM = IniFile::iniFileTof32(
      shipIniFilename,
      "azimuthDriveClutchEngageRPM");  // the engine revs above which the clutch
                                       // will automatically engage
  azimuthDriveClutchDisengageRPM = IniFile::iniFileTof32(
      shipIniFilename,
      "azimuthDriveClutchDisengageRPM");  // the engine revs below which the
                                          // clutch will automatically disengage
  schottelMaxDegPerSecond = IniFile::iniFileTof32(
      shipIniFilename,
      "schottelMaxDegPerSecond");  // only really useful for the button control
  azimuthDriveMaxDegPerSecond = IniFile::iniFileTof32(
      shipIniFilename,
      "azimuthDriveMaxDegPerSecond");  // the maxiumum rate the Azimuth Drive
                                       // will turn
  thrustLeverMaxChangePerSecond = IniFile::iniFileTof32(
      shipIniFilename,
      "thrustLeverMaxChangePerSecond");  // only really useful for the button
                                         // control
  engineMaxChangePerSecond = IniFile::iniFileTof32(
      shipIniFilename, "engineMaxDegPerSecond");  // max change in engine
  // conventionally the azimuth drive turns in the same direction as the
  // shcottel control, however it is not always the case e.g. shetland trader
  if (IniFile::iniFileTou32(shipIniFilename,
                            "azimuthDriveSameDirectionAsSchottel") == 1) {
    azimuthDriveSameDirectionAsSchottel = true;
  } else {
    azimuthDriveSameDirectionAsSchottel = false;
  }
  // DEE NOV22 ^^^^ azimuth drive code
  rollAngle = 2 * IniFile::iniFileTof32(
                      shipIniFilename, "Swell");  // Roll Angle (deg @weather=1)
  pitchPeriod = IniFile::iniFileTof32(
      shipIniFilename,
      "PitchPeriod");  // Softcoded roll period Tr a function of the ships
                       // condition indpendant of Te, the wave encounter period
  pitchAngle =
      0.5 * IniFile::iniFileTof32(shipIniFilename,
                                  "Swell");  // Max pitch Angle (deg @weather=1)
  buffet = IniFile::iniFileTof32(shipIniFilename, "Buffet");
  depthSounder =
      (IniFile::iniFileTou32(shipIniFilename, "HasDepthSounder") == 1);
  maxSounderDepth = IniFile::iniFileTof32(shipIniFilename, "MaxDepth");
  gps = (IniFile::iniFileTou32(shipIniFilename, "HasGPS") == 1);
  bowThrusterPresent =
      (IniFile::iniFileTof32(shipIniFilename, "BowThrusterForce") > 0);
  sternThrusterPresent =
      (IniFile::iniFileTof32(shipIniFilename, "SternThrusterForce") > 0);
  turnIndicatorPresent =
      (IniFile::iniFileTou32(shipIniFilename, "HasRateOfTurnIndicator") == 1);
  bowThrusterMaxForce =
      IniFile::iniFileTof32(shipIniFilename, "BowThrusterForce");
  sternThrusterMaxForce =
      IniFile::iniFileTof32(shipIniFilename, "SternThrusterForce");
  bowThrusterDistance =
      IniFile::iniFileTof32(shipIniFilename, "BowThrusterDistance");
  sternThrusterDistance =
      IniFile::iniFileTof32(shipIniFilename, "SternThrusterDistance");
  // DEE_DEC22 vvvv  new ini variables, all optional
  dynamicsLateralDragA =
      IniFile::iniFileTof32(shipIniFilename, "DynamicsLateralDragA");
  dynamicsLateralDragB =
      IniFile::iniFileTof32(shipIniFilename, "DynamicsLateralDragB");
  cB = IniFile::iniFileTof32(shipIniFilename, "BlockCoefficient");

  aziToLengthRatio = IniFile::iniFileTof32(
      shipIniFilename,
      "AziToLengthRatio");  // DEE_DEC22 use 0..1, a proportion of ships length,
                            // so between 0 and 0.5 is a azimuth stern drive
                            // congiguration and between 0.5 and 1 is a tractor
                            // configuration
  maxSpeed = IniFile::iniFileTof32(shipIniFilename,
                                   "maxSpeedAhead");  // expressed in knots
  maxSpeed_mps = maxSpeed * 0.514444;  // expressed in metres per second
  // DEE_DEC22 ^^^^
  // Scale
  irr::f32 scaleFactor = IniFile::iniFileTof32(shipIniFilename, "ScaleFactor");
  irr::f32 yCorrection = IniFile::iniFileTof32(shipIniFilename, "YCorrection");
  angleCorrection = IniFile::iniFileTof32(shipIniFilename, "AngleCorrection");
  // DEE_DEC22 vvvv
  angleCorrectionRoll = 0;   // default value
  angleCorrectionPitch = 0;  // default value
  angleCorrectionRoll =
      IniFile::iniFileTof32(shipIniFilename, "AngleCorrectionRoll");
  angleCorrectionPitch =
      IniFile::iniFileTof32(shipIniFilename, "AngleCorrectionPitch");
  // DEE_DEC22 ^^^^
  // camera offset (in unscaled and uncorrected ship coords)
  irr::u32 numberOfViews = IniFile::iniFileTof32(shipIniFilename, "Views");
  if (numberOfViews == 0) {
    std::cerr
        << "Own ship: View positions can't be loaded. Please check ini file "
        << shipIniFilename << std::endl;
    exit(EXIT_FAILURE);
  }
  for (irr::u32 i = 1; i <= numberOfViews; i++) {
    irr::f32 camOffsetX =
        IniFile::iniFileTof32(shipIniFilename, IniFile::enumerate1("ViewX", i));
    irr::f32 camOffsetY =
        IniFile::iniFileTof32(shipIniFilename, IniFile::enumerate1("ViewY", i));
    irr::f32 camOffsetZ =
        IniFile::iniFileTof32(shipIniFilename, IniFile::enumerate1("ViewZ", i));
    bool highView =
        IniFile::iniFileTou32(shipIniFilename,
                              IniFile::enumerate1("ViewHigh", i)) == 1;
    views.push_back(irr::core::vector3df(scaleFactor * camOffsetX,
                                         scaleFactor * camOffsetY,
                                         scaleFactor * camOffsetZ));
    isHighView.push_back(highView);
  }

  // Radar Screen Resolutions
  screenDisplayPosition.X =
      IniFile::iniFileTof32(shipIniFilename, "RadarScreenX");
  screenDisplayPosition.Y =
      IniFile::iniFileTof32(shipIniFilename, "RadarScreenY");
  screenDisplayPosition.Z =
      IniFile::iniFileTof32(shipIniFilename, "RadarScreenZ");
  screenDisplaySize = IniFile::iniFileTof32(shipIniFilename, "RadarScreenSize");
  screenDisplayTilt = IniFile::iniFileTof32(shipIniFilename, "RadarScreenTilt");
  // Default position out of view if not set
  if (screenDisplayPosition.X == 0 && screenDisplayPosition.Y == 0 &&
      screenDisplayPosition.Z == 0) {
    screenDisplayPosition.Y = 500;
  }

  if (screenDisplaySize <= 0) {
    screenDisplaySize = 1;
  }
  screenDisplayPosition.X *= scaleFactor;
  screenDisplayPosition.Y *= scaleFactor;
  screenDisplayPosition.Z *= scaleFactor;
  screenDisplaySize *= scaleFactor;

  // Load the model

  irr::scene::IAnimatedMesh* shipMesh;

  // Check if the 'model' is actualy the string "360". If so, treat it as a 360
  // equirectangular panoramic image with transparency.
  is360textureShip = false;
  if (Utilities::hasEnding(ownShipFullPath, "360")) {
    is360textureShip = true;
  }

  // Set mesh vertical correction (world units)
  heightCorrection = yCorrection * scaleFactor;

  if (is360textureShip) {
    // make a dummy node, to which the views will be added as children
    shipMesh = smgr->addSphereMesh("Sphere", 1);
    ship = smgr->addAnimatedMeshSceneNode(shipMesh, 0, IDFlag_IsPickable,
                                          irr::core::vector3df(0, 0, 0));

    // Add child meshes for each
    for (int i = 0; i < views.size(); i++) {
      irr::scene::IAnimatedMesh* viewMesh = smgr->addSphereMesh(
          irr::io::path("Sphere") + irr::io::path(i), 5.0, 32, 32);
      smgr->getMeshManipulator()->flipSurfaces(viewMesh);

      // Angle correction
      irr::f32 panoRotationYaw = IniFile::iniFileTof32(
          shipIniFilename, IniFile::enumerate1("PanoRotationYaw", i + 1));
      irr::f32 panoRotationPitch = IniFile::iniFileTof32(
          shipIniFilename, IniFile::enumerate1("PanoRotationPitch", i + 1));
      irr::f32 panoRotationRoll = IniFile::iniFileTof32(
          shipIniFilename, IniFile::enumerate1("PanoRotationRoll", i + 1));

      irr::scene::IAnimatedMeshSceneNode* viewNode =
          smgr->addAnimatedMeshSceneNode(
              viewMesh, ship, -1, views.at(i) / scaleFactor,
              irr::core::vector3df(panoRotationPitch, panoRotationYaw,
                                   panoRotationRoll));

      std::string panoPath =
          basePath + IniFile::iniFileToString(
                         shipIniFilename, IniFile::enumerate1("Pano", i + 1));
      irr::video::ITexture* texture360 =
          device->getVideoDriver()->getTexture(panoPath.c_str());

      if (texture360 != 0) {
        viewNode->setMaterialTexture(0, texture360);
        viewNode->getMaterial(0).getTextureMatrix(0).setTextureScale(-1.0, 1.0);
      }

      viewNode->setMaterialFlag(irr::video::EMF_FOG_ENABLE, true);
      viewNode->setMaterialFlag(
          irr::video::EMF_NORMALIZE_NORMALS,
          true);  // Normalise normals on scaled meshes, for correct lighting
      // Set lighting to use diffuse and ambient, so lighting of untextured
      // models works
      if (viewNode->getMaterialCount() > 0) {
        for (irr::u32 mat = 0; mat < viewNode->getMaterialCount(); mat++) {
          viewNode->getMaterial(mat).MaterialType =
              irr::video::EMT_TRANSPARENT_ALPHA_CHANNEL;
          viewNode->getMaterial(mat).ColorMaterial =
              irr::video::ECM_DIFFUSE_AND_AMBIENT;
        }
      }
      if (i > 0) {
        viewNode->setVisible(false);  // Hide all except the first one
      }
    }
  } else {
    shipMesh = smgr->getMesh(ownShipFullPath.c_str());
    // Make mesh scene node
    if (shipMesh == 0) {
      // Failed to load mesh - load with dummy and continue
      device->getLogger()->log("Failed to load own ship model:");
      device->getLogger()->log(ownShipFullPath.c_str());
      shipMesh = smgr->addSphereMesh("Dummy name");
    }

    // If any part is partially transparent, make it fully transparent (for
    // bridge windows etc!)
    if (IniFile::iniFileTou32(shipIniFilename, "MakeTransparent") == 1) {
      for (irr::u32 mb = 0; mb < shipMesh->getMeshBufferCount(); mb++) {
        if (shipMesh->getMeshBuffer(mb)->getMaterial().DiffuseColor.getAlpha() <
            255) {
          // Hide this mesh buffer by scaling to zero size
          smgr->getMeshManipulator()->scale(shipMesh->getMeshBuffer(mb),
                                            irr::core::vector3df(0, 0, 0));
        }
      }
    }
    ship = smgr->addAnimatedMeshSceneNode(shipMesh, 0, IDFlag_IsPickable,
                                          irr::core::vector3df(0, 0, 0));

    ship->setMaterialFlag(irr::video::EMF_FOG_ENABLE, true);
    ship->setMaterialFlag(
        irr::video::EMF_NORMALIZE_NORMALS,
        true);  // Normalise normals on scaled meshes, for correct lighting
    // Set lighting to use diffuse and ambient, so lighting of untextured models
    // works
    if (ship->getMaterialCount() > 0) {
      for (irr::u32 mat = 0; mat < ship->getMaterialCount(); mat++) {
        ship->getMaterial(mat).MaterialType =
            irr::video::EMT_TRANSPARENT_VERTEX_ALPHA;
        ship->getMaterial(mat).ColorMaterial =
            irr::video::ECM_DIFFUSE_AND_AMBIENT;
      }
    }
  }

  ship->setScale(irr::core::vector3df(scaleFactor, scaleFactor, scaleFactor));
  ship->setPosition(irr::core::vector3df(0, heightCorrection, 0));

  length = ship->getBoundingBox().getExtent().Z *
           scaleFactor;  // Store length for basic collision calculation
  width = ship->getBoundingBox().getExtent().X *
          scaleFactor;  // Store length for basic collision calculation
  height = ship->getBoundingBox().getExtent().Y * scaleFactor;

  // DEE_DEC22 ---------- End of reading in information from .ini files and
  // ownShipData

  // DEE_DEC22 Start setting defaults and sanity checks on parameters
  irr::f32 breadth;
  breadth = width;  //  DEE_DEC22 just a more usual term that I am less likely
                    //  to forget !
  irr::f32 seawaterDensity =
      1024;  // define seawater density in kg / m^3 could parametarise this for
             // dockwater and freshwater
  irr::f32 draught =
      -1 * yCorrection * scaleFactor;  // DEE_DEC22 i think thats right perhaps
                                       // there needs to be a SF im not sure

  if (rollPeriod == 0) {
    rollPeriod = 8;  // default to a roll period of 8 seconds if unspecified
  }

  if (pitchPeriod == 0) {
    pitchPeriod =
        12;  // default to a roll periof of 12 seconds if unspecified DEE_DEC22
             // make a function of weather strength direction and Ixx
  }

  // Default buffet Period DEE_DEC22 to do make this a function of Izz and
  // weather strength perhaps direction too
  buffetPeriod = 8;  // Yaw period (s)

  // Default for maxSounderDepth
  if (maxSounderDepth < 1) {
    maxSounderDepth = 100;
  }  // Default

  // Defaults to max rudder speed
  if (rudderMaxSpeed == 0) {
    rudderMaxSpeed = 30;  // default to an almost instentaeous rudder
  }
  rudderMinAngle = -30;
  rudderMaxAngle = 30;

  // set if Azimuth drive
  if (IniFile::iniFileTou32(shipIniFilename, "AzimuthDrive") == 1) {
    azimuthDrive = true;
  } else {
    azimuthDrive = false;
    aziToLengthRatio = 0;
  }

  // Set azimuth drives independent
  azimuth1Master = false;
  azimuth2Master = false;

  // Set defaults for values that shouldn't be zero
  if (asternEfficiency == 0) {
    asternEfficiency = 1;
  }

  if (length <= 0) {
    device->getLogger()->log("Invalid length from boat.x");
  }
  if (breadth <= 0) {
    device->getLogger()->log("Invalid breadth from boat.x");
  }
  if (draught <= 0) {
    device->getLogger()->log("Invalid draught from boat.ini YCorrection");
  }

  // DEE_DEC22 calculate moment arm from amidships , approximated to the
  // centre of lateral water resistance
  // to the azipods, hence the arm of
  // the lateral thrust from them
  aziDriveLateralLeverArm =
      length *
      (0.5 - aziToLengthRatio);  // this takes care of the sense of the turn
                                 // as the sign changes if tractor configuration

  // DEE_DEC22 set ships mass and inertia from displacement and length breadth
  // draught and Cb (block coefficient)
  if (cB > 0) {
    // ie. the block coefficient has been defined so it overrides any
    // declaration of mass or inertia
    device->getLogger()->log(
        "cB defined in boat.ini mass and inertia shall be calculated from "
        "dimensions");
    shipMass = seawaterDensity * length * breadth * height * cB;  // kg
    std::cout << "SHIP DIMENSIONS: l: " << length << ", b: " << breadth
              << ", h: " << height << std::endl;
    std::cout << "COMPUTED SHIP MASS: " << shipMass << std::endl;
    Izz = shipMass * ((length * length) + (breadth * breadth)) /
          12;  // inertia about the vertical zz axis kg m^2
  } else {     // BlockCoefficient , cB has not been defined in the ini file
               // so use the declared values for mass and inertia
    if (shipMass <= 0) {
      // no or an invalid ship mass is declared in .ini
      shipMass = 10000000;  // default value for ship mass
      device->getLogger()->log(
          "cB not defined in boat.ini mass declared in boat.ini invalid "
          "reverting to default");
    } else {
      device->getLogger()->log(
          "cB not defined in boat.ini mass declared in boat.ini used");
    }
    if (Izz <= 0) {
      // no or an invalid inertia is declared in .ini
      Izz = 145960000000;  // based on a 120m 10,000T 14m breadth cargo vessel
                           // units are kg m^2
      device->getLogger()->log(
          "cB not defined in boat.ini and invalid interia in boat.ini Default "
          "Inertia Izz of 145960000000 kg m^2 used");
    } else {
      device->getLogger()->log(
          "cB not defined in boat.ini, Inertia declared in boat.ini used");
    }
  }
  // DEE_DEC22 by this point we have a value for Izz so we can calculate values
  // for Iyy and Ixx.
  //		 I know that the Ixx is an under calculation because the actual
  // geometry is not 		 very symmetric however it is not orders of
  // magnitude out
  Ixx = Izz * ((length * length) + (draught * draught)) /
        ((length * length) + (breadth * breadth));
  Iyy = Izz * ((breadth * breadth) + (draught * draught)) /
        ((breadth * breadth) + (length * length));
  // DEE_DEC22 end of mass and inertia definitions

  // DEE_DEC22 if maxSpeed is defined in the .ini file then calculate drag
  // coefficients from geometry and maxForce
  //           otherwise use the values declared in ini file.   So maxSpeed, a
  //           new parameter overrides the
  //	         drag coefficient
  if (maxSpeed > 0) {  // note maxSpeed_mps had already been calculated which is
                       // maxSpeed in metres per second
    device->getLogger()->log(
        "maxSpeed is defined in boat.ini so the defined drag parameters shall "
        "be calculated from ship dimensions");
    dynamicsSpeedA = seawaterDensity * breadth * draught;
    dynamicsSpeedB = 0;  // not worth bothering with take drag as proportional
                         // to the square of speed only
                         // lateral drag calculated later
    // angular drag coefficient again only calculate the coefficient thats
    // multiplied by angularvelocity sqaured
    dynamicsTurnDragA =
        2 * draught * (length * length * length) * seawaterDensity / 3;
    dynamicsTurnDragB = 0;  // neglected
    // note this models the turning drag of the ship as if it was a vertical
    // plane turning in the water about a vertical axis

    maxForce = dynamicsSpeedA * (maxSpeed_mps * maxSpeed_mps) +
               dynamicsSpeedB * maxSpeed_mps;
  } else {
    // maxSpeed is either invalid or undefined so use the drag coefficients from
    // the ini file
    device->getLogger()->log(
        "maxSpeed is undefined in boat.ini so the defined drag parameters from "
        "there shall be used");
  }

  // DEE_DEC22 rather than default to *10 it should default to *(length /
  // breadth) as draught is the same for all
  if (dynamicsLateralDragA == 0) {
    dynamicsLateralDragA =
        dynamicsSpeedA *
        (length / breadth);  // this is the constant for the sqaure of speed
                             // so is proportional to the area pusing into the
                             // water for a ship that is longer than wide then
                             // it should be greater than the axial constant
  }
  if (dynamicsLateralDragB == 0) {  // DEE_DEC22 this plays such a small part is
                                    // it even worth including ?
    dynamicsLateralDragB = dynamicsSpeedB * (breadth / length);
  }

  if (propellorSpacing == 0) {
    singleEngine = true;
    maxForce *=
        0.5;  // Internally simulated with two equal engines, so halve the value
    device->getLogger()->log("Single engine");
  } else {
    singleEngine = false;
  }

  // Todo: Missing:
  // CentrifugalDriftEffect	DEE_DEC22 possibly taken into account with new
  // turn modelling PropWalkDriftEffect	DEE_DEC22 depends on direction of
  // propellor
  // Windage			DEE_DEC22 simple model would be above sea
  // profile area
  // * windspeed ^2 WindageTurnEffect		DEE_DEC22 arm is diff between
  //					centre of lateral (water) pressure
  //					and center of lateral (wind) pressure
  // Also:
  // DeviationMaximum		DEE_DEC22 could use World Magnetic Model WMM for
  // variation also DeviationMaximumHeading		  could use a phase and
  // amplitude shifted SIN function for this ?Depth ?AngleCorrection

  // Start in engine control mode
  controlMode = MODE_ENGINE;

  // calculate max speed from dynamics parameters
  //  DEE this looks like it is in knots and not metres per second
  maxSpeedAhead =
      ((-1 * dynamicsSpeedB) + sqrt((dynamicsSpeedB * dynamicsSpeedB) -
                                    4 * dynamicsSpeedA * -2 * maxForce)) /
      (2 * dynamicsSpeedA);
  maxSpeedAstern =
      ((-1 * dynamicsSpeedB) +
       sqrt((dynamicsSpeedB * dynamicsSpeedB) -
            4 * dynamicsSpeedA * -2 * maxForce * asternEfficiency)) /
      (2 * dynamicsSpeedA);

  // Calculate engine speed required - the port and stbd engine speeds get send
  // back to the GUI with updateGuiData.

  // DEE_NOV22 	todo need to ammend this for azimuth drive, perhaps if initial
  // speed is 0 then clutched out both azis and rpm at idle and lever back
  //		otherwise some interpolation needed to get something reasonable

  model->setPortEngine(requiredEngineProportion(
      axialSpd));  // Set via model to ensure sound volume is set too
  model->setStbdEngine(requiredEngineProportion(
      axialSpd));  // Set via model to ensure sound volume is set too
  // DEE_NOV22 suggest that change in volume of engine noise is appropriate for
  // controllable pitch propellor vessels only DEE_NOV22 where the engine runs
  // at a constant rpm ( so a shaft generator can be run directly off the shaft
  // ) DEE_NOV22 the thrust control being as a result of adjustment of the
  // propellor blades. DEE_NOV22 Otherwise, where thrust is a direct function of
  // engine rpm then the frequency of the sound should vary with DEE_NOV22 each
  // engine's rpm
  rudder = 0;
  rateOfTurn = 0;

  followUpRudderWorking = true;
  rudderPump1Working = true;  // Fully working rudder actuation
  rudderPump2Working = true;  // Fully working rudder actuation

  // set initial pitch and roll
  pitch = 0;
  roll = 0;
  waveHeightFiltered = 0;

  // Initialise
  bowThruster = 0;
  sternThruster = 0;

  cog = 0;
  sog = 0;

  sternThrusterRate = 0;
  bowThrusterRate = 0;

  rudder = 0;
  wheel = 0;

  portAzimuthAngle = 0;
  stbdAzimuthAngle = 0;

  // DEE_NOV22 vvvv Azimuth Drive code
  // calculate some parameters here for computational efficiency
  // HERE
  irr::f32 idleEngine =
      (azimuthDriveEngineIdleRPM) /
      (maxEngineRevs);  // DEE_NOV22 calculate idle engine expressed as (0..1)
                        // as opposed to RPM

  // DEE_NOV22 initialise some new variables here
  portSchottel = 90;   // port schottel dead ahead
  stbdSchottel = 270;  // stbd schottel dead ahead
  portThrustLever =
      0.1;  // port thrust lever  DEBUG set to 0.1 TODO calculate the setting
            // that corresponds to the engine intital ship speed
  stbdThrustLever = 0.1;  // stbd thrust lever  DEBUG set to 0.1
  portClutch = false;     // Port clutch is disengaged DEGUG set to engaged
  stbdClutch = false;     // Starboard clutch is disengaged

  // DEE_NOV22 DEBUG setting thrust lever to 0.1 and clutches disengaged

  // DEE_NOV22 ^^^^

  buoyCollision = false;
  otherShipCollision = false;

  // Detect sample points for terrain interaction here (think separately about
  // how to do this for 360 models, probably with a separate collision model)
  // Add a triangle selector
  irr::scene::ITriangleSelector* selector = smgr->createTriangleSelector(ship);
  if (selector) {
    device->getLogger()->log("Created triangle selector");
    ship->setTriangleSelector(selector);
  }

  ship->updateAbsolutePosition();

  irr::core::aabbox3df boundingBox = ship->getTransformedBoundingBox();
  irr::f32 minX = boundingBox.MinEdge.X;
  irr::f32 maxX = boundingBox.MaxEdge.X;
  irr::f32 minY = boundingBox.MinEdge.Y;
  irr::f32 maxY = boundingBox.MaxEdge.Y;
  irr::f32 minZ = boundingBox.MinEdge.Z;
  irr::f32 maxZ = boundingBox.MaxEdge.Z;

  // Grid from below looking up
  for (int i = 0; i < numberOfContactPoints.X; i++) {
    for (int j = 0; j < numberOfContactPoints.Z; j++) {
      irr::f32 xTestPos = minX + (maxX - minX) * (irr::f32)i /
                                     (irr::f32)(numberOfContactPoints.X - 1);
      irr::f32 zTestPos = minZ + (maxZ - minZ) * (irr::f32)j /
                                     (irr::f32)(numberOfContactPoints.Z - 1);

      irr::core::line3df
          ray;  // Make a ray. This will start outside the mesh, looking in
      ray.start.X = xTestPos;
      ray.start.Y = minY - 0.1;
      ray.start.Z = zTestPos;
      ray.end = ray.start;
      ray.end.Y = maxY + 0.1;

      // Check the ray and add the contact point if it exists
      addContactPointFromRay(ray);
    }
  }

  // Grid from ahead/astern
  for (int i = 0; i < numberOfContactPoints.X; i++) {
    for (int j = 0; j < numberOfContactPoints.Y; j++) {
      irr::f32 xTestPos = minX + (maxX - minX) * (irr::f32)i /
                                     (irr::f32)(numberOfContactPoints.X - 1);
      irr::f32 yTestPos = minY + (maxY - minY) * (irr::f32)j /
                                     (irr::f32)(numberOfContactPoints.Y - 1);

      irr::core::line3df
          ray;  // Make a ray. This will start outside the mesh, looking in
      ray.start.X = xTestPos;
      ray.start.Y = yTestPos;
      ray.start.Z = maxZ + 0.1;
      ray.end = ray.start;
      ray.end.Z = minZ - 0.1;

      // Check the ray and add the contact point if it exists
      addContactPointFromRay(ray);
      // swap ray direction and check again
      ray.start.Z = minZ - 0.1;
      ray.end.Z = maxZ + 0.1;
      addContactPointFromRay(ray);
    }
  }

  // Grid from side to side
  for (int i = 0; i < numberOfContactPoints.Z; i++) {
    for (int j = 0; j < numberOfContactPoints.Y; j++) {
      irr::f32 zTestPos = minZ + (maxZ - minZ) * (irr::f32)i /
                                     (irr::f32)(numberOfContactPoints.Z - 1);
      irr::f32 yTestPos = minY + (maxY - minY) * (irr::f32)j /
                                     (irr::f32)(numberOfContactPoints.Y - 1);

      irr::core::line3df
          ray;  // Make a ray. This will start outside the mesh, looking in
      ray.start.X = maxX + 0.1;
      ray.start.Y = yTestPos;
      ray.start.Z = zTestPos;
      ray.end = ray.start;
      ray.end.X = minX - 0.1;

      // Check the ray and add the contact point if it exists
      addContactPointFromRay(ray);
      // swap ray direction and check again
      ray.start.X = minX - 0.1;
      ray.end.X = maxX + 0.1;
      addContactPointFromRay(ray);
    }
  }

  // We don't want to do further triangle selection with the ship, so set the
  // selector to null
  ship->setTriangleSelector(0);
  device->getLogger()->log("Own ship points found: ");
  device->getLogger()->log(irr::core::stringw(contactPoints.size()).c_str());

  // our custom "submarine" specs
  ballastTankVolume =
      length * breadth * height *
      IniFile::iniFileTof32(shipIniFilename, "RelBallastTankVolume");
  ballastPumpRate = IniFile::iniFileTof32(shipIniFilename, "BallastPumpRate");
  displacement_mass = seawaterDensity * length * breadth * height;
  // approximate projected_area by using the lower face of the bounding box
  projected_area = length * breadth;
  ballastTankFillLevel = 0.0;
  ballastPump = 0.0;
  yAcceleration = 0.0;
  yVelocity = 0.0;
  buoyancy = -1.0;
  is_submerged = false;

  irr::f32 uw_axial_drag_mod =
      IniFile::iniFileTof32(shipIniFilename, "UnderWaterAxialDragModifier");
  irr::f32 uw_turn_drag_mod =
      IniFile::iniFileTof32(shipIniFilename, "UnderWaterTurnDragModifier");
  irr::f32 uw_lateral_drag_mod =
      IniFile::iniFileTof32(shipIniFilename, "UnderWaterLateralDragModifier");

  surfaceDynamicsSpeedA = dynamicsSpeedA;
  surfaceDynamicsSpeedB = dynamicsSpeedB;
  surfaceDynamicsTurnDragA = dynamicsTurnDragA;
  surfaceDynamicsTurnDragB = dynamicsTurnDragB;
  surfaceDynamicsLateralDragA = dynamicsLateralDragA;
  surfaceDynamicsLateralDragB = dynamicsLateralDragB;
  underWaterDynamicsSpeedA = dynamicsSpeedA * uw_axial_drag_mod;
  underWaterDynamicsSpeedB = dynamicsSpeedB * uw_axial_drag_mod;
  underWaterDynamicsTurnDragA = dynamicsTurnDragA * uw_turn_drag_mod;
  underWaterDynamicsTurnDragB = dynamicsTurnDragB * uw_turn_drag_mod;
  underWaterDynamicsLateralDragA = dynamicsLateralDragA * uw_lateral_drag_mod;
  underWaterDynamicsLateralDragB = dynamicsLateralDragB * uw_lateral_drag_mod;
}

void OwnShip::addContactPointFromRay(irr::core::line3d<irr::f32> ray) {
  irr::core::vector3df intersection;
  irr::core::triangle3df hitTriangle;

  irr::scene::ISceneNode* selectedSceneNode =
      device->getSceneManager()
          ->getSceneCollisionManager()
          ->getSceneNodeAndCollisionPointFromRay(
              ray,
              intersection,  // This will be the position of the collision
              hitTriangle,   // This will be the triangle hit in the collision
              IDFlag_IsPickable,  // (bitmask)
              0);                 // Check all nodes

  if (selectedSceneNode) {
    ContactPoint contactPoint;
    contactPoint.position = intersection;
    contactPoint.normal = hitTriangle.getNormal().normalize();
    contactPoint.position.Y -= heightCorrection;  // Adjust for height
                                                  // correction

    // Find an internal node position, i.e. a point at which a ray check for
    // internal intersection can start
    ray.start = contactPoint.position;
    ray.end = ray.start - 100 * contactPoint.normal;
    // Check for the internal node
    selectedSceneNode =
        device->getSceneManager()
            ->getSceneCollisionManager()
            ->getSceneNodeAndCollisionPointFromRay(
                ray,
                intersection,  // This will be the position of the collision
                hitTriangle,   // This will be the triangle hit in the collision
                IDFlag_IsPickable,  // (bitmask)
                0);                 // Check all nodes

    if (selectedSceneNode) {
      contactPoint.internalPosition = intersection;
      contactPoint.internalPosition.Y -=
          heightCorrection;  // Adjust for height correction

      // Find cross product, for torque component
      irr::core::vector3df crossProduct =
          contactPoint.position.crossProduct(contactPoint.normal);
      contactPoint.torqueEffect = crossProduct.Y;

      // Store the contact point that we have found
      contactPoints.push_back(contactPoint);  // Store

      // Debugging
      // contactDebugPoints.push_back(device->getSceneManager()->addSphereSceneNode(0.1));
      // contactDebugPoints.push_back(device->getSceneManager()->addCubeSceneNode(0.1));
    }
  }
}

void OwnShip::setRateOfTurn(
    irr::f32 rateOfTurn)  // Sets the rate of turn (used when controlled as
                          // secondary)
{
  controlMode = MODE_AUTO;  // Switch to controlled mode
  this->rateOfTurn = rateOfTurn;
}

irr::f32 OwnShip::getRateOfTurn() const { return rateOfTurn; }

void OwnShip::setBallastTankPump(irr::f32 pump) {
  ballastPump = std::min(1.0f, std::max(-1.0f, pump));
}

irr::f32 OwnShip::getBallastTankPump() { return ballastPump; }

void OwnShip::setRudder(irr::f32 rudder) {
  controlMode = MODE_ENGINE;  // Switch to engine and rudder mode
  // Set the rudder (-ve is port, +ve is stbd)
  this->rudder = rudder;
  if (this->rudder < rudderMinAngle) {
    this->rudder = rudderMinAngle;
  }
  if (this->rudder > rudderMaxAngle) {
    this->rudder = rudderMaxAngle;
  }
}

void OwnShip::setWheel(irr::f32 wheel, bool force) {
  controlMode = MODE_ENGINE;  // Switch to engine and rudder mode
  // Set the wheel (-ve is port, +ve is stbd), unless follow up rudder isn't
  // working (overrideable with 'force')
  if (followUpRudderWorking || force) {
    this->wheel = wheel;
    if (this->wheel < rudderMinAngle) {
      this->wheel = rudderMinAngle;
    }
    if (this->wheel > rudderMaxAngle) {
      this->wheel = rudderMaxAngle;
    }
  }
}

void OwnShip::setPortAzimuthAngle(irr::f32 angle) {
  if (azimuth2Master) {
    // Don't do anything if the other azimuth control is master
    return;
  }
  this->portAzimuthAngle = angle;
}

// DEE_NOV22 vvvv follow up port azimuth drive in response to port schottel

void OwnShip::followupPortAzimuthDrive() {
  // DEE_NOV22 vvvv temporary variables could probably be done a lot more neatly
  irr::f32 newPortAzimuthAngle;
  // DEE_NOV22 ^^^^

  //    azimuthDriveSameDirectionAsSchottel=true; //DEBUG
  if (azimuthDriveSameDirectionAsSchottel) {
    // conventional schottel control
    commandedPortAngle = portSchottel;
  } else {
    // mirrored schottel control like Shetland Trader and other Lass vessels.
    // Steers like a wheel rather than a tiller going ahead
    commandedPortAngle =
        360 -
        portSchottel;  // DEE_NOV22 shouldnt have to error trap 0 > commanded  <
                       // 360 because portSchottel already checked for that
  }

  // keep commanded angles in the range 0..360 floating point
  if (commandedPortAngle < 0) {
    commandedPortAngle = commandedPortAngle + 360;
  }
  if (commandedPortAngle > 360) {
    commandedPortAngle = commandedPortAngle - 360;
  }

  irr::f32 maxChangeInAzimuthDriveAngleThisCycle =
      deltaTime *
      azimuthDriveMaxDegPerSecond;  // DEE_NOV22 the maximum rotation possible
                                    // in this cycle

  // DEE_NOV22 now determine if it needs to move clockwise or anticlockwise

  bool clockwise = false;  // temporary

  int shortestAngularDistance = int(commandedPortAngle - portAzimuthAngle) %
                                360;  // approximated by integer
  if (shortestAngularDistance > 180) {
    shortestAngularDistance = shortestAngularDistance - 360;
  }  // anticlockwise
  if (shortestAngularDistance < -180) {
    shortestAngularDistance = shortestAngularDistance + 360;
  }  // clockwise

  //

  if (abs(shortestAngularDistance) >
      int(maxChangeInAzimuthDriveAngleThisCycle)) {  // the azimuth drive cannot
                                                     // reach the commanded
                                                     // angle on this cycle so
                                                     // move it as much as
                                                     // possible towards it
    int tempInt = int(commandedPortAngle - portAzimuthAngle);
    if (tempInt < 0) {
      tempInt = tempInt + 360;
    }
    if (shortestAngularDistance > 0)  // clockwise or anticlockwise
    {
      // DEE_NOV22 clockwise rotation
      newPortAzimuthAngle =
          portAzimuthAngle + maxChangeInAzimuthDriveAngleThisCycle;
      if (newPortAzimuthAngle > 360) {
        newPortAzimuthAngle = newPortAzimuthAngle - 360;
      }

    } else {
      // DEE_NOV22 anticlockwise rotation
      // DEE_NOV22 in hindsight this is probably not needed but it does make it
      // more robust
      newPortAzimuthAngle =
          portAzimuthAngle - maxChangeInAzimuthDriveAngleThisCycle;
      if (newPortAzimuthAngle < 0) {
        newPortAzimuthAngle = newPortAzimuthAngle + 360;
      }
    }       // end if clockwise or anticlockwise followup
  } else {  // the azimuth drive can reach the commanded angle on this cycle
    newPortAzimuthAngle = commandedPortAngle;
  }

  if (azimuth2Master) {
    // If the other azimuth drive is master then take the
    // azimuth angle from the other the other engine
    newPortAzimuthAngle = stbdAzimuthAngle;
  }  // end if azimuth2Master

  setPortAzimuthAngle(newPortAzimuthAngle);

}  // end of followup code for the port azimuth

void OwnShip::followupStbdAzimuthDrive() {
  // DEE_NOV22 vvvv temporary variables could probably be done a lot more neatly
  irr::f32 newStbdAzimuthAngle;
  // DEE_NOV22 ^^^^

  if (azimuthDriveSameDirectionAsSchottel) {
    // conventional schottel control
    commandedStbdAngle = stbdSchottel;
  } else {
    // mirrored schottel control like Shetland Trader and other Lass vessels.
    // Steers like a wheel rather than a tiller going ahead
    commandedStbdAngle =
        360 -
        stbdSchottel;  // DEE_NOV22 shouldnt have to error trap 0 > commanded  <
                       // 360 because stbdSchottel already checked for that
  }

  // keep commanded angles in the range 0..360 floating point
  if (commandedStbdAngle < 0) {
    commandedStbdAngle = commandedStbdAngle + 360;
  }
  if (commandedStbdAngle > 360) {
    commandedStbdAngle = commandedStbdAngle - 360;
  }

  irr::f32 maxChangeInAzimuthDriveAngleThisCycle =
      deltaTime *
      azimuthDriveMaxDegPerSecond;  // DEE_NOV22 the maximum rotation possible
                                    // in this cycle

  // DEE_NOV22 now determine if it needs to move clockwise or anticlockwise
  int shortestAngularDistance = int(commandedStbdAngle - stbdAzimuthAngle) %
                                360;  // approximated by integer
  if (shortestAngularDistance > 180) {
    shortestAngularDistance = shortestAngularDistance - 360;
  }  // anticlockwise
  if (shortestAngularDistance < -180) {
    shortestAngularDistance = shortestAngularDistance + 360;
  }  // clockwise

  if (abs(shortestAngularDistance) >
      int(maxChangeInAzimuthDriveAngleThisCycle)) {  // the azimuth drive cannot
                                                     // reach the commanded
                                                     // angle on this cycle so
                                                     // move it as much as
                                                     // possible towards it
    int tempInt = int(commandedStbdAngle - stbdAzimuthAngle);
    if (tempInt < 0) {
      tempInt = tempInt + 360;
    }
    if (shortestAngularDistance > 0)  // clockwise or anticlockwise
    {
      // DEE_NOV22 clockwise rotation
      newStbdAzimuthAngle =
          stbdAzimuthAngle + maxChangeInAzimuthDriveAngleThisCycle;
      if (newStbdAzimuthAngle > 360) {
        newStbdAzimuthAngle = newStbdAzimuthAngle - 360;
      }

    } else {
      // DEE_NOV22 anticlockwise rotation
      // DEE_NOV22 in hindsight this is probably not needed but it does make it
      // more robust
      newStbdAzimuthAngle =
          stbdAzimuthAngle - maxChangeInAzimuthDriveAngleThisCycle;
      if (newStbdAzimuthAngle < 0) {
        newStbdAzimuthAngle = newStbdAzimuthAngle + 360;
      }
    }       // end if clockwise or anticlockwise followup
  } else {  // the azimuth drive can reach the commanded angle on this cycle
    newStbdAzimuthAngle = commandedStbdAngle;
  }

  if (azimuth1Master) {
    // If the other azimuth drive is master then take the
    // azimuth angle from the other the other engine
    // stbdAzimuthAngle = portAzimuthAngle;
    newStbdAzimuthAngle = portAzimuthAngle;
  }  // end if azimuth1Master

  setStbdAzimuthAngle(newStbdAzimuthAngle);

}  // end of followup code for azimuth drive
// DEE_NOV22 ^^^^

void OwnShip::setStbdAzimuthAngle(irr::f32 angle) {
  if (azimuth1Master) {
    // Don't do anything if the other azimuth control is master
    return;
  }

  if (azimuth2Master) {
    // If linked, set the other engine
    portAzimuthAngle = stbdAzimuthAngle;
  }

  this->stbdAzimuthAngle = angle;
}

void OwnShip::setAzimuth1Master(bool isMaster) {
  // Set if azimuth 1 should also control azimuth 2
  // DEE_NOV22 azimuth drive 1 being the port azimuth drive
  // DEE_NOV22 I've never seen this 'master' configuration in practice, the only
  // thing
  //		 I've seen resembling it is the autopilot controlling either or
  // both azimuth drives
  azimuth1Master = isMaster;
  if (azimuth1Master) {
    // Can't have both as master
    azimuth2Master = false;

    // Update controls to match current azimuth1 settings
    stbdAzimuthAngle = portAzimuthAngle;
    stbdEngine = portEngine;
  }
}

void OwnShip::setAzimuth2Master(bool isMaster) {
  // Set if azimuth 2 should also control azimuth 1
  // DEE_NOV22 azimuth drive 2 being the starboard azimuth drive
  azimuth2Master = isMaster;
  if (azimuth2Master) {
    // Can't have both as master
    azimuth1Master = false;

    // Update controls to match current azimuth2 settings
    portAzimuthAngle = stbdAzimuthAngle;
    portEngine = stbdEngine;
  }
}

bool OwnShip::getAzimuth1Master() const { return azimuth1Master; }

bool OwnShip::getAzimuth2Master() const { return azimuth2Master; }

void OwnShip::setPortEngine(irr::f32 port) {
  if (azimuthDrive && azimuth2Master) {
    // Don't do anything if the other azimuth control is master
    return;
  }

  controlMode = MODE_ENGINE;  // Switch to engine and rudder mode

  // DEE_NOV22
  if (azimuthDrive) {
    portEngine = port;  // 0..1
    if (portEngine > 1) {
      portEngine = 1;
    }
    if (portEngine < 0) {
      portEngine = 0;
    }

  } else {
    // this is not an azimuth drive

    portEngine = port;  //+-1
    if (portEngine > 1) {
      portEngine = 1;
    }
    if (portEngine < -1) {
      portEngine = -1;
    }
  }  // end if this is not an azimuth drive

  portEngine = port;
}  // end setPortEngine

void OwnShip::setStbdEngine(irr::f32 stbd) {
  if (azimuthDrive && azimuth1Master) {
    // Don't do anything if the other azimuth control is master
    return;
  }

  controlMode = MODE_ENGINE;  // Switch to engine and rudder mode

  // DEE_NOV22
  if (azimuthDrive) {
    stbdEngine = stbd;  // 0..1
    if (stbdEngine > 1) {
      stbdEngine = 1;
    }
    if (stbdEngine < 0) {
      stbdEngine = 0;
    }

  } else {
    // this is not an azimuth drive

    stbdEngine = stbd;  //+-1
    if (stbdEngine > 1) {
      stbdEngine = 1;
    }
    if (stbdEngine < -1) {
      stbdEngine = -1;
    }
  }  // end if this is not an azimuth drive

  stbdEngine = stbd;

  // DEE_NOV22 ^^^^

  if (azimuthDrive && azimuth2Master) {  // DEE_NOV22 comment azimuth2 being the
                                         // stbd azimuth drive
    // If azimuth controls linked
    portEngine = stbdEngine;
  }
}  // end setStbdEngine

void OwnShip::setBowThruster(irr::f32 proportion) {
  // Proportion is -1 to +1
  bowThruster = proportion;
  if (bowThruster > 1) {
    bowThruster = 1;
  }
  if (bowThruster < -1) {
    bowThruster = -1;
  }
}

void OwnShip::setSternThruster(irr::f32 proportion) {
  // Proportion is -1 to +1
  sternThruster = proportion;
  if (sternThruster > 1) {
    sternThruster = 1;
  }
  if (sternThruster < -1) {
    sternThruster = -1;
  }
}

void OwnShip::setBowThrusterRate(irr::f32 bowThrusterRate) {
  // Sets the rate of increase of bow thruster, used for joystick button control
  this->bowThrusterRate = bowThrusterRate;
}

void OwnShip::setSternThrusterRate(irr::f32 sternThrusterRate) {
  // Sets the rate of increase of stern thruster, used for joystick button
  // control
  this->sternThrusterRate = sternThrusterRate;
}

void OwnShip::setRudderPumpState(int whichPump, bool rudderPumpState) {
  if (whichPump == 1) {
    rudderPump1Working = rudderPumpState;
  }
  if (whichPump == 2) {
    rudderPump2Working = rudderPumpState;
  }
}

bool OwnShip::getRudderPumpState(int whichPump) const {
  if (whichPump == 1) {
    return rudderPump1Working;
  }
  if (whichPump == 2) {
    return rudderPump2Working;
  }
  return false;
}

void OwnShip::setFollowUpRudderWorking(bool followUpRudderWorking) {
  // Sets if the normal (follow up) rudder is working
  this->followUpRudderWorking = followUpRudderWorking;
}

// DEE_NOV22 vvvv
bool OwnShip::getFollowUpRudderWorking() { return followUpRudderWorking; }
// DEE_NOV22 ^^^^

irr::f32 OwnShip::getPortEngine() const {  // DEE_NOV22 note range -1 .. 1 needs
                                           // to be 0 .. 1 for azimuth drive
  return portEngine;
}

irr::f32 OwnShip::getStbdEngine() const { return stbdEngine; }

irr::f32 OwnShip::getBowThruster() const { return bowThruster; }

irr::f32 OwnShip::getSternThruster() const { return sternThruster; }

irr::f32 OwnShip::getPortEngineRPM() const {
  return portEngine * maxEngineRevs;
}

irr::f32 OwnShip::getStbdEngineRPM() const {
  return stbdEngine * maxEngineRevs;
}

irr::f32 OwnShip::getRudder() const { return rudder; }

irr::f32 OwnShip::getWheel() const { return wheel; }

irr::f32 OwnShip::getPortAzimuthAngle() const {
  return portAzimuthAngle;  // Angle in degrees
}

irr::f32 OwnShip::getStbdAzimuthAngle() const {
  return stbdAzimuthAngle;  // Angle in degrees
}

irr::f32 OwnShip::getPitch() const { return pitch; }

irr::f32 OwnShip::getRoll() const { return roll; }

std::string OwnShip::getBasePath() const { return basePath; }

irr::core::vector3df OwnShip::getScreenDisplayPosition() const {
  return screenDisplayPosition;
}

irr::f32 OwnShip::getScreenDisplaySize() const { return screenDisplaySize; }

irr::f32 OwnShip::getScreenDisplayTilt() const { return screenDisplayTilt; }

bool OwnShip::isSingleEngine() const { return singleEngine; }

bool OwnShip::isAzimuthDrive() const { return azimuthDrive; }

// DEE_NOV22 vvvv
// Azimuth Drive code added

// Schottels

bool OwnShip::isConventionalAzidriveSchottel()
    const {  // returns true if the azimuth drive turns in the same direction as
             // the schottel , conventionally this is so, but not always e.g.
             // Shetland Trader
  return azimuthDriveSameDirectionAsSchottel;
}

void OwnShip::setPortSchottel(
    irr::f32 portAngle) {  // sets port schottel angle -ve anticlockwise
  // DEE_NOV22    Im not sure this set is used anywhere but I will leave it in
  //		because in the future it may be needed for some sort of
  //		more realistic autopilot function
  this->portSchottel = portAngle;
}

void OwnShip::setStbdSchottel(
    irr::f32
        stbdAngle) {  // sets starboard azimuth drive angle -ve anticlockwise
  this->stbdSchottel = stbdAngle;
}

irr::f32 OwnShip::getPortSchottel()
    const {  // gets port      azimuth drive angle -ve anticlockwise
  return portSchottel;
}

irr::f32 OwnShip::getStbdSchottel()
    const {  // gets starboard azimuth drive angle -ve anticlockwise
  return stbdSchottel;
}

// DEE_NOV22 Clutches .... not necessarily particular to Azimuth drive ships
//			since when stopping and starting an engine then it
//			must be clutched out.  Also if apparatus or divers
//			are being deployed overboard then to stop the propellor
//			turning without switching off the engine then it is
//			necessary to clutch out.

void OwnShip::setPortClutch(
    bool myPortClutch) {  // sets port clutch true is clutched in false is
                          // clutched out
  portClutch = myPortClutch;
}

void OwnShip::setStbdClutch(
    bool myStbdClutch) {  // sets starboard clutch true is clutched in false is
                          // clutched out
  stbdClutch = myStbdClutch;
}

bool OwnShip::getPortClutch() {  // gets port clutch perhaps have synonym
                                 // isPortClutched() todo
  return portClutch;
}

bool OwnShip::getStbdClutch() {  // gets stbd clutch perhaps have synonym
                                 // isPortClutched() todo
  return stbdClutch;
}

void OwnShip::engagePortClutch() {  // engage port clutch  separate key method
                                    // that calls this to facilitate Non follow
                                    // up in the future
  this->setPortClutch(true);
  // DEE_NOV22 future development make a clunk sound on engaging
  //	     	"	"	if the engine is running too fast then snap the
  // propshaft 		"	"	if the engine is running too slow then
  // stall the engine
}

void OwnShip::disengagePortClutch() {  // disengage port clutch separate key
                                       // method that calls this to facilitate
                                       // Non follow up / follow up
  // DEE_NOV22 futre development over rev the engine if disengaged at too high a
  // speed ? a bit pedantic perhaps
  setPortClutch(false);
}

void OwnShip::engageStbdClutch() {  // engage stbd clutch as above re key
                                    // bindings
  this->setStbdClutch(true);
  // DEE_NOV22 future development make a clunk sound on engaging
  //	     	"	"	if the engine is running too fast then snap the
  // propshaft 		"	"	if the engine is running too slow then
  // stall the engine
}

void OwnShip::disengageStbdClutch() {  // disengage stbd clutch as above re key
                                       // bindings
  this->setStbdClutch(false);
}

void OwnShip::setPortThrustLever(
    irr::f32 thrustLever) {  // sets port thrust lever 0..1
  if (thrustLever < 0) {
    thrustLever = 0;
  }
  if (thrustLever > 1) {
    thrustLever = 1;
  }
  this->portThrustLever = thrustLever;
}

void OwnShip::setStbdThrustLever(
    irr::f32 thrustLever) {  // sets stbd thrust lever 0..1
  if (thrustLever < 0) {
    thrustLever = 0;
  }
  if (thrustLever > 1) {
    thrustLever = 1;
  }
  this->stbdThrustLever = thrustLever;
}

irr::f32
OwnShip::getPortThrustLever() {  // gets position of port thrust lever 0..1
  return this->portThrustLever;
}

irr::f32
OwnShip::getStbdThrustLever() {  // gets position of stbd thrust lever 0..1
  return this->stbdThrustLever;
}

// todo put this in to SimulationModel and into EventCapture, Add entries into
// ini file and read them in to the model todo future have the optional capacity
// to declutch any propellor from the engine, especially controlable pitch
// propellors  just like RV Prince Madog todo add a schottel position indicator
// to azipod display todo add a thrust lever indicator to azipod display

// below are for button control

void OwnShip::btnIncrementPortSchottel() {  // turns port schottel clockwise in
                                            // response to a key press D
  newPortSchottel =
      getPortSchottel() + getLastDeltaTime() * schottelMaxDegPerSecond;
  if (newPortSchottel >
      360) {  // DEE_NOV22 for the case that it has gone all the way around
    this->newPortSchottel = newPortSchottel - 360;
  }
  setPortSchottel(newPortSchottel);
}

void OwnShip::btnDecrementPortSchottel() {  // turns port schottel anticlockwise
                                            // by one degree in response to a
                                            // key press A
  newPortSchottel =
      getPortSchottel() - getLastDeltaTime() * schottelMaxDegPerSecond;
  if (newPortSchottel <
      0) {  // DEE_NOV22 for the case that it has gone all the way around
    this->newPortSchottel = newPortSchottel + 360;
  }
  setPortSchottel(newPortSchottel);
}

void OwnShip::btnIncrementStbdSchottel() {  // turns stbd schottel clockwise by
                                            // one degree in response to a key
                                            // press J
  newStbdSchottel =
      getStbdSchottel() + getLastDeltaTime() * schottelMaxDegPerSecond;
  if (newStbdSchottel >
      360) {  // DEE_NOV22 for the case that it has gone all the way around
    this->newStbdSchottel = newStbdSchottel - 360;
  }
  setStbdSchottel(newStbdSchottel);
}

void OwnShip::btnDecrementStbdSchottel() {  // turns port schottel anticlockwise
                                            // by one degree in response to a
                                            // key press L
  newStbdSchottel =
      getStbdSchottel() - getLastDeltaTime() * schottelMaxDegPerSecond;
  if (newStbdSchottel <
      0) {  // DEE_NOV22 for the case that it has gone all the way around
    newStbdSchottel = newStbdSchottel + 360;
  }
  setStbdSchottel(newStbdSchottel);
}  // DEE End btnDecrementStbdSchottel()

// DEE_NOV22 note that the above controls the position of the schottels and not
// the azidrives that's later

void OwnShip::btnEngagePortClutch() {  // engage port clutch todo key binding
                                       // for this
  engagePortClutch();
}

void OwnShip::btnDisengagePortClutch() {  // disengage port clutch todo key
                                          // binding for this
  disengagePortClutch();
}

void OwnShip::btnEngageStbdClutch() {  // engage stbd clutch todo key binding
                                       // for this
  engageStbdClutch();
}

void OwnShip::btnDisengageStbdClutch() {  // disengage stbd clutch todo key
                                          // binding for this
  disengageStbdClutch();
}

void OwnShip::btnIncrementPortThrustLever() {  // increments the port thrust
                                               // lever
  setPortThrustLever(getPortThrustLever() +
                     getLastDeltaTime() * thrustLeverMaxChangePerSecond);
}

void OwnShip::btnDecrementPortThrustLever() {  // decrements the port thrust
                                               // lever
  irr::f32 tempvar;
  tempvar =
      getPortThrustLever() - getLastDeltaTime() * thrustLeverMaxChangePerSecond;
  setPortThrustLever(tempvar);
}

void OwnShip::btnIncrementStbdThrustLever() {  // increments the stbd thrust
                                               // lever
  setStbdThrustLever(getStbdThrustLever() +
                     getLastDeltaTime() * thrustLeverMaxChangePerSecond);
}

void OwnShip::btnDecrementStbdThrustLever() {  // decrements the stbd thrust
                                               // lever
  setStbdThrustLever(getStbdThrustLever() -
                     getLastDeltaTime() * thrustLeverMaxChangePerSecond);
}

// DEE_NOV22 ^^^^

bool OwnShip::isBuoyCollision() const { return buoyCollision; }

bool OwnShip::isOtherShipCollision() const { return otherShipCollision; }

irr::f32 OwnShip::requiredEngineProportion(irr::f32 speed) {
  irr::f32 proportion = 0;
  if (speed >= 0) {
    proportion = (dynamicsSpeedA * speed * speed + dynamicsSpeedB * speed) /
                 (2 * maxForce);
  } else {
    proportion =
        (-1 * dynamicsSpeedA * speed * speed + dynamicsSpeedB * speed) /
        (2 * maxForce * asternEfficiency);
  }
  return proportion;
}

// DEE_NOV22 vvvv
// the delta time of the previous cycle, good enough for movement of shcottels
// and levers

irr::f32 OwnShip::getLastDeltaTime() { return deltaTime; }

void OwnShip::setLastDeltaTime(irr::f32 myDeltaTime) {
  deltaTime = myDeltaTime;
}

// DEE_NOV22 ^^^^

void OwnShip::update(irr::f32 deltaTime, irr::f32 scenarioTime,
                     irr::f32 tideHeight, irr::f32 weather) {
  // DEE_NOV22 vvvv
  setLastDeltaTime(deltaTime);
  // DEE_NOV22 ^^^^

  // dynamics: hdg in degrees, axialSpd lateralSpd in m/s. Internal units all SI
  if (controlMode == MODE_ENGINE) {
    // Check depth and update collision response forces and torque
    irr::f32 groundingAxialDrag = 0;
    irr::f32 groundingLateralDrag = 0;
    irr::f32 groundingTurnDrag = 0;
    collisionDetectAndRespond(
        groundingAxialDrag, groundingLateralDrag,
        groundingTurnDrag);  // The drag values will get modified by this call

    // Update bow and stern thrusters, if being controlled by joystick buttons
    bowThruster += deltaTime * bowThrusterRate;
    if (bowThruster > 1) {
      bowThruster = 1;
    }
    if (bowThruster < -1) {
      bowThruster = -1;
    }
    sternThruster += deltaTime * sternThrusterRate;
    if (sternThruster > 1) {
      sternThruster = 1;
    }
    if (sternThruster < -1) {
      sternThruster = -1;
    }

    // DEE_DEC2022
    // portThrust stbdThrust  meaning changed to the magnitude of thrust
    // portAxialThrust to mean component of thrust along the axis of the vessel
    // portLateralThrust to mean component of thrust athwartships
    //
    // remodel parameters so that the lever arm of forces act about L/2
    // which is an approximation of the centre of lateral resistance
    // and the axis of the vessel
    //
    // Calculate Mass and Inertia about the vertical axis (Izz) from ini
    // parameters Cb block coefficient ususally 0.87 for a general cargo ship
    // draught, bread, length and seawater density to obtain the displacement
    // (mass) calculate Izz = mass * (length^2 * breadth^2) / 12 for the future
    // modelling of rolling and pitching then we can also calculate Iyy = mass *
    // (centreGravityTemporal^2 + breadth^2) / 12, for rolling with a
    // guesstimate made of the height of centreGravity Ixx = mass *
    // (centreGravityTemporal^2 + length^2) / 12, for pitching motion

    // Update axialSpd and hdg with rudder and engine controls - assume two
    // engines, should also work with single engine
    irr::f32 portThrust = 0;  // DEE_DEC22 changed meaning to scalar not vector
    irr::f32 stbdThrust = 0;  // DEE_DEC22 changed meaning to scalar not vector

    // DEE_DEC22 vvvv
    irr::f32 portAxialThrust = 0;
    irr::f32 stbdAxialThrust = 0;
    irr::f32 portLateralThrust = 0;
    irr::f32 stbdLateralThrust = 0;
    irr::f32 portTemporalThrust =
        0;  // probably temporal not needed at present but for future use as
            // trim of outboard engine
    irr::f32 stbdTemporalThrust =
        0;  // and for some configuration of sails and kites

    irr::f32 axialThrust = 0;     // sum of axial thrusts
    irr::f32 lateralThrust = 0;   // sum of lateral thrusts
    irr::f32 temporalThrust = 0;  // sum of temporal thrusts
                                  // DEE_DEV22 ^^^^

    if (azimuthDrive) {
      // DEE_NOV22 todo have some efficiency function to reduce thrust when azi
      // wash is obstructed by other azi and hull DEE_NOV22 Follow up behaviour
      // for azimuth drive direction led by schottels DEE_NOV22 consider adding
      // a conditional which would allow for modelling non follow up control
      // buttons / keys only

      followupPortAzimuthDrive();
      followupStbdAzimuthDrive();

      // DEE_NOV22 Follow up behaviour for engine led by thrust lever
      // DEE_NOV22 consider adding a conditional which would allow for modelling
      // non follow up control buttons / keys only
      maxChangeInEngineThisCycle = deltaTime * thrustLeverMaxChangePerSecond;

      // Port Engine
      if (portEngine < portThrustLever) {
        // Port engine setting needs to increase
        if (portEngine < (portThrustLever - maxChangeInEngineThisCycle)) {
          // the port engine cannot attain the commanded setting on this cycle
          // so increment by the most it can
          newPortEngine = portEngine + maxChangeInEngineThisCycle;
        } else {
          // the port engine can attain the commanded setting on this cycle so
          // set it so
          newPortEngine = portThrustLever;
        }
        if (newPortEngine < idleEngine) {
          // prevent a change that would make the engine speed reduce below
          // idling rpm
          newPortEngine = idleEngine;
        }  // end if command less than idle engine speed

      } else {
        // Port engine setting needs to decrease
        if (portEngine >
            (portThrustLever +
             maxChangeInEngineThisCycle)) {  // the port engine cannot attain
                                             // the commanded setting on this
                                             // cycle so decrement by the most
                                             // it can
          newPortEngine = portEngine - maxChangeInEngineThisCycle;
        } else {
          // the port engine can attain the commanded setting on this cycle so
          // set it so
          newPortEngine = portThrustLever;
        }
        if (newPortEngine < idleEngine) {
          // prevent a change that would make the engine speed reduce below
          // idling rpm
          newPortEngine = idleEngine;
        }
      }  // end if increase port engine else decrease port engine
      // there is another possibility and that is portEngine == portThrustLever
      // in which case nothing needs to happen so theres no code
      if (newPortEngine < idleEngine) {
        // prevent a change that would make the engine speed reduce below idling
        // rpm
        newPortEngine = idleEngine;
      }  // end if commanded engine speed is less than engine speed
      setPortEngine(newPortEngine);

      // DEE_NOV22 end of port engine code

      // DEE_NOV22 Port propulsion system Clutch
      // todo DEE_NOV22 this needs to be within a conditional if follow up
      // control (FuC), else clutch must be manual
      bool ifFuC = true;  // todo DEE_NOV22 obviously this will need to be
                          // controlled ... bodging it for development purposes
      if (ifFuC) {
        // Follow up Control with automatic clutch
        if ((portEngine * maxEngineRevs) > azimuthDriveClutchEngageRPM) {
          // the clutch should be in the engaged position
          engagePortClutch();
        }
        if ((portEngine * maxEngineRevs) < azimuthDriveClutchDisengageRPM) {
          // the clutch should be in the disengaged position
          disengagePortClutch();
        }
      } else {
        // Non follow up , emergency steering so the clutch is controlled by
        // buttons todo DEE_NOV22 as yet unassigned
        ;
      }
      // DEE_NOV22 end of port propulsion system clutch

      // Starboard Engine code
      if (stbdEngine < stbdThrustLever) {
        // Starboard engine setting needs to increase
        if (stbdEngine < (stbdThrustLever - maxChangeInEngineThisCycle)) {
          // the stbd engine cannot attain the commanded setting on this cycle
          // so increment by the most it can
          newStbdEngine = stbdEngine + maxChangeInEngineThisCycle;
        } else {
          // the stbd engine can maintain the commanded setting on this cycle so
          // set it so
          newStbdEngine = stbdThrustLever;
        }
      } else {
        // Starboard engine setting needs to decrease
        if (stbdEngine > (stbdThrustLever + maxChangeInEngineThisCycle)) {
          // the stbd engine cannot attain the commanded setting on this cycle
          // so decrement by the most it can
          newStbdEngine = stbdEngine - maxChangeInEngineThisCycle;
        } else {
          // the starboard engine can attain the commanded setting on this cycle
          // so set it so
          newStbdEngine = stbdThrustLever;
        }
      }
      // there is another possibility and that is stbdEngine == stbdThrustLever
      // in which case nothing needs to happen
      if (newStbdEngine < idleEngine) {
        // prevent a change that would make the engine speed reduce below idling
        // rpm
        newStbdEngine = idleEngine;
      }  // end if commanded engine speed is less than engine speed
      setStbdEngine(newStbdEngine);
      // End of starboard engine code

      // Starboard propulsion system Clutch

      // todo DEE_NOV22 this needs to be within a conditional if follow up
      // control (FuC), else clutch must be manual there is probably already an
      // variable available for follow up non follow up DEE_NOV22 todo
      bool isFuC = true;  // todo DEE_NOV22 obviously this will need to be
                          // controlled ... bodging it for development purposes
      if (isFuC) {
        // Follow up Control with automatic clutch
        if ((stbdEngine * maxEngineRevs) > azimuthDriveClutchEngageRPM) {
          // the clutch should be in the engaged position
          if (stbdClutch == false) {
            // the starboard clutch is not engaged then engage it the reason
            // it's as a separate method is so that in the future a 'clunk'
            // sound can be added.  Also useful when key button control in non
            // follow up mode
            engageStbdClutch();
          }  // end if the stbd clutch is disengaged then engage it
        }    // end if the stbd engine in running at greater than the clutch in
             // speed
        if ((stbdEngine * maxEngineRevs) < azimuthDriveClutchDisengageRPM) {
          // the clutch should be in the disengaged position
          if (stbdClutch == true) {
            // the starboard clutch is engaged but it needs to be disengaged,
            // the reason it's as a separate method is to that in a future a
            // 'little clunk' sound can be added. Also usefeul when key button
            // control in non follow up mode
            disengageStbdClutch();
          }  // end if the stbd engine is engaged then disengage it
        }    // end if the stbd engine is running below clutch out speed
      } else {
        // Non follow up , emergency steering so the clutch is controlled by
        // buttons todo DEE_NOV22 as yet unassigned
        ;
      }

      // DEE_NOV22 ^^^^

      // DEE_NOV22 vvvv thrust is only produced when the azimuth drive is
      // clutched in, the below code does
      //		      not consider the automatic clutching in and out so
      // should work in a future 		      non follow up mode (for
      // azis) or ships equipped with manual clutches (RV Prince Madog
      // and mosts CPP vessels) as the only arguments are whether an engine is
      // clutched in or not 		      , engine setting, max force,
      // direction.  The direction factor could be taken care of with a
      // conditional branch (i.e. if not an azi then axial thrust = maxforce *
      // engine (no need for angle)
      // 		      or a an if clutch in put in the "Conventional
      // Controls" branch

      // DEE_DEC22 vvvv not commenting out old code in this section as it gets
      // too messy to be readable DEE_DEC22 future todo add an engine trim
      // component to model temporal thrust i.e. portTemporalThrust
      //		it is omitted at present due to thrusts being generally
      // in the plane axial-lateral 		engine trim is the angular
      // difference of the direction of engine thrust from the axial-lateral
      // plane

      if (portClutch) {
        portThrust = portEngine * maxForce;
        // refers to the magnitude of force produced by the engine irrespective
        // of direction
      } else {
        portThrust = 0;
      }  // end if port Azi is clutched in
      portAxialThrust =
          portThrust * cos(portAzimuthAngle * irr::core::DEGTORAD);
      portLateralThrust =
          portThrust * sin(portAzimuthAngle * irr::core::DEGTORAD);

      if (stbdClutch) {
        stbdThrust = stbdEngine * maxForce;
      } else {
        stbdThrust = 0;
      }  // end if stbd azi is clutched in
      stbdAxialThrust =
          stbdThrust * cos(stbdAzimuthAngle * irr::core::DEGTORAD);
      stbdLateralThrust =
          stbdThrust * sin(stbdAzimuthAngle * irr::core::DEGTORAD);

      // DEE_NOV22 ^^^^
      // DEE_DEC22 ^^^^

    } else {  // End if Azimuth Drive

      // Conventional controls
      // DEE_DEC22 vvvv
      portThrust = portEngine * maxForce;
      stbdThrust = stbdEngine * maxForce;

      if (portThrust < 0) {
        portThrust *= asternEfficiency;
      }
      if (stbdThrust < 0) {
        stbdThrust *= asternEfficiency;
      }

      // For conventional engine, all the thrust is axial
      portAxialThrust = portThrust;
      stbdAxialThrust = stbdThrust;

      // DEE_DEC22 ^^^^

    }  // DEE_NOV22 end if conventional controls then calculate thrust thus

    // Ignore stbd slider if single engine (internally modelled as 2 engines,
    // each with half the max force)
    if (singleEngine) {
      stbdThrust = portThrust;
      // DEE_DEC22 vvvv
      stbdAxialThrust = portAxialThrust;
      stbdLateralThrust = portLateralThrust;
      // stbdTemporalThrust = portTemporalThrust; // DEE_DEC22 not yet
      // implemented DEE_DEC22 ^^^^
    }

    // DEE_DEC22 vvvv old code deleted not commented out for clarity
    //		drag	replaced by	axialDrag
    //		spd	replaced by	axialSpd but spd is updated for the
    // ancestor
    // object 		acceleration	"	axialAcceleration
    // groundingDrag " groundingAxialDrag

    irr::f32 axialDrag;
    if (axialSpd < 0) {  // Compensate for loss of sign when squaring
      axialDrag =
          -1 * dynamicsSpeedA * axialSpd * axialSpd + dynamicsSpeedB * axialSpd;
    } else {
      axialDrag =
          dynamicsSpeedA * axialSpd * axialSpd + dynamicsSpeedB * axialSpd;
    }
    irr::f32 axialAcceleration =
        (portAxialThrust + stbdAxialThrust - axialDrag - groundingAxialDrag) /
        shipMass;
    // Check acceleration plausibility (not more than 1g = 9.81ms/2)
    if (axialAcceleration > 9.81) {
      axialAcceleration = 9.81;
    } else if (axialAcceleration < -9.81) {
      axialAcceleration = -9.81;
    }
    axialSpd += axialAcceleration * deltaTime;
    spd = axialSpd;  // for compatability with ancestor object
    // Also check speed for plausibility, limit to 50m/s
    if (axialSpd > 50) {
      axialSpd = 50;
    } else if (axialSpd < -50) {
      axialSpd = -50;
    }

    // DEE_DEC22 not commenting out old code for clarity
    // Lateral dynamics
    lateralThrust = bowThruster * bowThrusterMaxForce +
                    sternThruster * sternThrusterMaxForce;
    if (azimuthDrive) {
      if (singleEngine) {
        // double effect of 'port' engine
        // DEE_DEC22 never seen a vessel with just one azi, however with a limit
        // of rotation then its similar to an outboard engine DEE_NOV22 vvvv
        // comment out original and replace with below lateralThrust +=
        // portEngine * maxForce * sin(portAzimuthAngle*irr::core::DEGTORAD);
        if (portClutch) {
          // port engine is clutched in , as only a single engine let it act for
          // both
          lateralThrust += 2 * portLateralThrust;
        }
      } else {  // DEE else two independent azimuth drives
        // DEE_NOV22 there are two azi each should be calculated independently
        lateralThrust += portLateralThrust +
                         stbdLateralThrust;  // sin takes care of the +ve -ve
      }  // DEE_NOV22 end if else single engine azimuth drive else twin azimuth
         // drive
    }    // DEE_NOV22 end if azimuth drive

    // DEE_DEC22 todo upon load ship then calculate DONE earlier
    // 	dynamicsLateralDragA as dynamicsSpeedA * (L/B)
    //	dynamicsLateralDragB as dynamicsSppedB * (B/L)
    //	the submerged depth being constant we dont need to take it into account

    // DEE_DEC22 this does the sway of the vessel when turning it probably
    // models centrifugal drift too
    irr::f32 lateralDrag;
    if (lateralSpd < 0) {  // Compensate for loss of sign when squaring
      lateralDrag = -1 * dynamicsLateralDragA * lateralSpd * lateralSpd +
                    dynamicsLateralDragB * lateralSpd;
    } else {
      lateralDrag = dynamicsLateralDragA * lateralSpd * lateralSpd +
                    dynamicsLateralDragB * lateralSpd;
    }  //  end if lateral drag
    irr::f32 lateralAcceleration =
        (lateralThrust - lateralDrag - groundingLateralDrag) / shipMass;
    // std::cout << "Lateral acceleration (m/s2): " << lateralAcceleration <<
    // std::endl; Check acceleration plausibility (not more than 1g = 9.81ms/2)
    if (lateralAcceleration > 9.81) {
      lateralAcceleration = 9.81;
    } else if (lateralAcceleration < -9.81) {
      lateralAcceleration = -9.81;
    }
    lateralSpd += lateralAcceleration * deltaTime;
    // Also check speed for plausibility, limit to 50m/s
    if (lateralSpd > 50) {
      lateralSpd = 50;
    } else if (lateralSpd < -50) {
      lateralSpd = -50;
    }

    // Turn dynamics
    //  Azimuth Drive
    if (azimuthDrive) {
      rudderTorque = 0;
      engineTorque = 0;  // Will build up from components
                         // clockwise is +ve

      if (singleEngine) {
        // Double the 'port' engine effect, as we model as if we have two half
        // power engines in the same place
        engineTorque += portAxialThrust * propellorSpacing / 2.0;
        engineTorque -= portLateralThrust * aziDriveLateralLeverArm;
        engineTorque *= 2;
      } else {
        // twin azimuth drives so - the axial thrust for starboard engine and
        // +ve the lateral as angle same direction
        engineTorque += portAxialThrust * propellorSpacing / 2.0;
        engineTorque -= portLateralThrust * aziDriveLateralLeverArm;
        engineTorque -= stbdAxialThrust * propellorSpacing / 2.0;
        engineTorque -= stbdLateralThrust * aziDriveLateralLeverArm;
      }
    } else {
      // Regular rudder
      if ((portThrust + stbdThrust) > 0) {
        rudderTorque = rudder * axialSpd * rudderA +
                       rudder * (portThrust + stbdThrust) * rudderB;
      } else {
        rudderTorque = rudder * axialSpd * rudderA +
                       rudder * (portThrust + stbdThrust) *
                           rudderBAstern;  // Reduced effect of rudder when
                                           // engines engaged astern
      }
      // Engine
      engineTorque =
          (portThrust * propellorSpacing - stbdThrust * propellorSpacing) /
          2.0;  // propspace is spacing between propellors, so halve to get
                // moment arm
    }

    // Prop walk
    irr::f32 propWalkTorquePort, propWalkTorqueStbd;
    if (portThrust > 0) {
      propWalkTorquePort =
          1 * propWalkAhead *
          (portThrust / maxForce);  // Had modification for 'invertspeed'
    } else {
      propWalkTorquePort = 1 * propWalkAstern * (portThrust / maxForce);
    }
    if (stbdThrust > 0) {
      propWalkTorqueStbd =
          -1 * propWalkAhead *
          (stbdThrust / maxForce);  // Had modification for 'invertspeed'
    } else {
      propWalkTorqueStbd = -1 * propWalkAstern * (stbdThrust / maxForce);
    }
    if (singleEngine) {
      // Special case for single engine, as we are just controlling the port
      // engine for the internal model 2* because the internal model is two
      // engines with zero spacing and half power each.
      propWalkTorque = 2 * propWalkTorquePort;
    } else {
      propWalkTorque = propWalkTorquePort + propWalkTorqueStbd;
    }
    // Thrusters
    irr::f32 thrusterTorque;
    thrusterTorque =
        bowThruster * bowThrusterMaxForce * bowThrusterDistance -
        sternThruster * sternThrusterMaxForce * sternThrusterDistance;
    // Turn drag
    if (rateOfTurn < 0) {
      dragTorque = -1 * (-1 * dynamicsTurnDragA * rateOfTurn * rateOfTurn +
                         dynamicsTurnDragB * rateOfTurn);
    } else {
      dragTorque = -1 * (dynamicsTurnDragA * rateOfTurn * rateOfTurn +
                         dynamicsTurnDragB * rateOfTurn);
    }
    // Turn dynamics

    irr::f32 angularAcceleration =
        (rudderTorque + engineTorque + propWalkTorque + thrusterTorque +
         dragTorque - groundingTurnDrag) /
        Izz;

    rateOfTurn += angularAcceleration * deltaTime;  // Rad/s
    // check plausibility for rate of turn, limit to ~4Pi rad/s
    if (rateOfTurn > 12) {
      rateOfTurn = 2;
    } else if (rateOfTurn < -12) {
      rateOfTurn = -12;
    }

    // apply buffeting to rate of turn - TODO: Check the integrals from this to
    // work out if the end magnitude is right
    if (!is_submerged) {
      rateOfTurn += irr::core::DEGTORAD * buffet * weather *
                    cos(scenarioTime * 2 * PI / buffetPeriod) *
                    ((irr::f32)std::rand() / RAND_MAX) * deltaTime;  // Rad/s
    }

    // Apply turn
    hdg += rateOfTurn * deltaTime * irr::core::RADTODEG;  // Deg

    // Limit rudder rate of turn
    irr::f32 MaxRudderInDtime =
        rudder + rudderMaxSpeed * deltaTime *
                     (rudderPump1Working * 0.5 + rudderPump2Working * 0.5);
    irr::f32 MinRudderInDtime =
        rudder - rudderMaxSpeed * deltaTime *
                     (rudderPump1Working * 0.5 + rudderPump2Working * 0.5);
    if (wheel > MaxRudderInDtime) {
      rudder =
          MaxRudderInDtime;  // rudder as far to starboard as time will allow
    } else {
      if (wheel > MinRudderInDtime) {
        rudder = wheel;  // rudder can turn to the wheel setting
      } else {
        rudder = MinRudderInDtime;  // rudder as far to port as time will allow
      }
    }

  } else  // End of engine mode
  {
    // MODE_AUTO
    if (!positionManuallyUpdated) {
      // Apply rate of turn
      hdg += rateOfTurn * deltaTime * irr::core::RADTODEG;  // Deg
    }
  }

  // Normalise heading
  while (hdg >= 360) {
    hdg -= 360;
  }
  while (hdg < 0) {
    hdg += 360;
  }

  irr::f32 xChange = 0;
  irr::f32 zChange = 0;

  // move, according to heading and speed
  if (!positionManuallyUpdated) {  // If the position has already been updated,
                                   // skip (for this loop only)
    // DEE_DEC22
    // I am assuming that the datum is the stern of the ship on its centreline,
    // so the gps position should be corrected to its antenna position on the
    // actual ship. Similar info is transmitted on AIS also however the AIS may
    // have its own dedicated GPS antenna.  We'll assume it doesnt so need to
    // look at the NEMA code

    xChange = sin(hdg * irr::core::DEGTORAD) * axialSpd * deltaTime +
              cos(hdg * irr::core::DEGTORAD) * lateralSpd * deltaTime;
    zChange = cos(hdg * irr::core::DEGTORAD) * axialSpd * deltaTime -
              sin(hdg * irr::core::DEGTORAD) * lateralSpd * deltaTime;
    // Apply tidal stream, based on our current absolute position
    irr::core::vector2df stream = model->getTidalStream(
        model->getLong(), model->getLat(), model->getTimestamp());
    if (getDepth() > 0) {
      irr::f32 streamScaling =
          fmin(1, getDepth());  // Reduce effect as water gets shallower
      xChange += stream.X * deltaTime * streamScaling;
      zChange += stream.Y * deltaTime * streamScaling;
    }
  } else {
    positionManuallyUpdated = false;
  }

  xPos += xChange;
  zPos += zChange;

  if (deltaTime > 0) {
    // Speed over ground
    sog = pow((pow(xChange, 2) + pow(zChange, 2)), 0.5) /
          deltaTime;  // speed over ground in m/s

    // Course over ground
    if (xChange != 0 || zChange != 0) {
      cog = atan2(xChange, zChange) * irr::core::RADTODEG;
      while (cog >= 360) {
        cog -= 360;
      }
      while (cog < 0) {
        cog += 360;
      }
    } else {
      cog = 0;
    }
  }  // if paused, leave cog & sog unchanged.

  // std::cout << "CoG: " << cog << " SoG: " << sog << std::endl;

  // Apply up/down motion from waves, with some filtering
  irr::f32 timeConstant =
      0.5;  // Time constant in s; TODO: Make dependent on vessel size
  irr::f32 factor = deltaTime / (timeConstant + deltaTime);
  waveHeightFiltered =
      (1 - factor) * waveHeightFiltered +
      factor * model->getWaveHeight(
                   xPos, zPos);  // TODO: Check implementation of simple filter!
  irr::f32 surfaceyPos = tideHeight + heightCorrection + waveHeightFiltered;

  if (ballastTankVolume > 0) {
    // we're simulating a submarine

    // update the fill level of the ballast tank
    ballastTankFillLevel +=
        ((ballastPump * ballastPumpRate) / ballastTankVolume) * deltaTime;
    ballastTankFillLevel = std::min(std::max(ballastTankFillLevel, 0.0f), 1.0f);

    irr::f32 current_mass =
        shipMass + (ballastTankFillLevel * ballastTankVolume * RHO_SW) +
        ((1 - ballastTankFillLevel) * ballastTankVolume * RHO_AIR);
    // compute the vertical acceleration, assuming constant water density
    // regardless of depth (positive acceleration is down)
    irr::f32 acceleration =
        ((current_mass - displacement_mass) * GRAVITY) / current_mass;

    buoyancy = displacement_mass / current_mass;

    // compute current terminal velocity (again assuming constant water density)
    // assuming an aeorynamic coefficient of 1.05 (cubic shape)
    // use the same terminal velocity for both directions
    irr::f32 terminal_velocity =
        std::sqrt((2 * (std::abs(current_mass - displacement_mass) * GRAVITY)) /
                  (RHO_SW * projected_area * 1.05));

    yVelocity = yVelocity + acceleration * deltaTime;

    if (yVelocity < 0.0) {
      yVelocity = std::max(yVelocity, -1.0f * terminal_velocity);
    } else {
      yVelocity = std::min(yVelocity, terminal_velocity);
    }

    // invert the velocity sign, compute new position
    yPos = yPos - (yVelocity * deltaTime);

    /*
    std::cout << "======================" << std::endl;
    std::cout << "tank: " << ballastTankFillLevel << std::endl;
    std::cout << "current_mass: " << current_mass << std::endl;
    std::cout << "acceleration: " << acceleration << std::endl;
    std::cout << "velocity: " << yVelocity << std::endl;
    std::cout << "terminal_velocity: " << terminal_velocity << std::endl;
    std::cout << "yPos:" << yPos << std::endl;
    */

    // yPos should never be higher than the surface, and never lower than
    // the seafloor. We "cheat" by actually stopping before the seafloor
    // to avoid weird interactions when colliding with it
    yPos = std::max(terrain->getHeight(xPos, zPos) + 1.0f,
                    std::min(surfaceyPos, yPos));

    // are we submerged now? used to adjust drag behavior,
    // and water transparency
    // FIXME: this doesn't take heightCorrection into account
    shipDepth = surfaceyPos - yPos;
    is_submerged = shipDepth > height;

    // std::cout << "yPos fixed:" << yPos << std::endl << std::endl;

    // if at least 1m below surface, change the opacity of the water
    if (is_submerged) {
      model->set_water_transparent(true);
    } else {
      model->set_water_transparent(false);
    }

    // adjust drag, we mess with the drag coefficients here to
    // avoid having to change the many places where they are used
    if (is_submerged) {
      dynamicsSpeedA = underWaterDynamicsSpeedA;
      dynamicsSpeedB = underWaterDynamicsSpeedB;
      dynamicsTurnDragA = underWaterDynamicsTurnDragA;
      dynamicsTurnDragB = underWaterDynamicsTurnDragB;
      dynamicsLateralDragA = underWaterDynamicsLateralDragA;
      dynamicsLateralDragB = underWaterDynamicsLateralDragB;
    } else {
      dynamicsSpeedA = surfaceDynamicsSpeedA;
      dynamicsSpeedB = surfaceDynamicsSpeedB;
      dynamicsTurnDragA = surfaceDynamicsTurnDragA;
      dynamicsTurnDragB = surfaceDynamicsTurnDragB;
      dynamicsLateralDragA = surfaceDynamicsLateralDragA;
      dynamicsLateralDragB = surfaceDynamicsLateralDragB;
    }

  } else {
    // we're always at the surface
    yPos = surfaceyPos;
  }

  // calculate pitch and roll - not linked to water/wave motion
  if (pitchPeriod > 0) {
    pitch = weather * pitchAngle * sin(scenarioTime * 2 * PI / pitchPeriod);
  }
  if (rollPeriod > 0) {
    roll = weather * rollAngle * sin(scenarioTime * 2 * PI / rollPeriod);
  }

  // attenuate the deeper we are, completely null out effects at 7m
  pitch *= std::pow(std::max(1.0f - (shipDepth / 7), 0.0f), 2);
  roll *= std::pow(std::max(1.0f - (shipDepth / 7), 0.0f), 2);

  // Set position & angles
  ship->setPosition(irr::core::vector3df(xPos, yPos, zPos));
  // DEE_DEC22 vvvv the original remains however this could be a replacement
  //    ship->setRotation(Angles::irrAnglesFromYawPitchRoll(hdg+angleCorrection,angleCorrectionPitch+pitch,angleCorrectionRoll+roll));
  //    // attempt 1
  //    ship->setRotation(irr::core::vector3df(angleCorrectionPitch+pitch,
  //    hdg+angleCorrection,angleCorrectionRoll+roll));
  ship->setRotation(Angles::irrAnglesFromYawPitchRoll(
      hdg + angleCorrection, pitch, roll));  // this is the original
  // DEE_DEC22 ^^^^
}

irr::f32 OwnShip::getCOG() const { return cog; }

irr::f32 OwnShip::getSOG() const {
  return sog;  // m/s
}

irr::f32 OwnShip::getDepth() const {
  return -1 * terrain->getHeight(xPos, zPos) + getPosition().Y;
}

irr::f32 OwnShip::getShipDepth() const { return shipDepth; }

irr::f32 OwnShip::getBuoyancy() const { return buoyancy; }

void OwnShip::collisionDetectAndRespond(irr::f32& reaction,
                                        irr::f32& lateralReaction,
                                        irr::f32& turnReaction) {
  reaction = 0;
  lateralReaction = 0;
  turnReaction = 0;

  buoyCollision = false;
  otherShipCollision = false;

  if (is360textureShip) {
    // Simple method, check contact at the depth point only, to be updated to
    // match updates in the main section

    irr::f32 localIntersection = 0;    // Ready to use
    irr::f32 localDepth = getDepth();  // Simple one point method
    // Contact model (proof of principle!)
    if (localDepth < 0) {
      localIntersection = -1 * localDepth;  // TODO: We should actually project
                                            // based on the gradient?
    }
    // Contact model (proof of principle!)
    if (localIntersection > 1) {
      localIntersection = 1;  // Limit
    }

    if (localIntersection > 0) {
      // slow down if aground
      if (axialSpd > 0) {
        axialSpd =
            fmin(0.1, axialSpd);  // currently hardcoded for 0.1 m/s, ~0.2kts
      }
      if (axialSpd < 0) {
        axialSpd = fmax(-0.1, axialSpd);
      }

      if (rateOfTurn > 0) {
        rateOfTurn =
            fmin(0.01, rateOfTurn);  // Rate of turn in rad/s, currently
                                     // hardcoded for 0.01 rad/s
      }
      if (rateOfTurn < 0) {
        rateOfTurn = fmax(-0.01, rateOfTurn);  // Rate of turn in rad/s
      }

      if (lateralSpd > 0) {
        lateralSpd = fmin(0.1, lateralSpd);
      }
      if (lateralSpd < 0) {
        lateralSpd = fmax(-0.1, lateralSpd);
      }
    }
  } else {
    // Normal ship model
    ship->updateAbsolutePosition();

    for (int i = 0; i < contactPoints.size(); i++) {
      irr::core::vector3df pointPosition = contactPoints.at(i).position;
      irr::core::vector3df internalPointPosition =
          contactPoints.at(i).internalPosition;

      // Rotate with own ship
      irr::core::matrix4 rot;
      rot.setRotationDegrees(ship->getRotation());
      rot.transformVect(pointPosition);
      rot.transformVect(internalPointPosition);

      pointPosition += ship->getAbsolutePosition();
      internalPointPosition += ship->getAbsolutePosition();

      irr::f32 localIntersection = 0;  // Ready to use

      // Find depth below the contact point
      irr::f32 localDepth =
          -1 * terrain->getHeight(pointPosition.X, pointPosition.Z) +
          pointPosition.Y;

      // Contact model (proof of principle!)
      if (localDepth < 0) {
        localIntersection =
            -1 * localDepth *
            abs(contactPoints.at(i)
                    .normal
                    .Y);  // Projected based on normal, so we get an estimate of
                          // the intersection normal to the contact point.
                          // Ideally this vertical component of the normal would
                          // react to the ship's motion, but probably not too
                          // important
      }

      // Also check contact with pickable scenery elements here (or other
      // ships?)
      irr::core::line3d<irr::f32> ray(internalPointPosition, pointPosition);
      irr::core::vector3df intersection;
      irr::core::triangle3df hitTriangle;
      irr::scene::ISceneNode* selectedSceneNode =
          device->getSceneManager()
              ->getSceneCollisionManager()
              ->getSceneNodeAndCollisionPointFromRay(
                  ray,
                  intersection,  // This will be the position of the collision
                  hitTriangle,   // This will be the triangle hit in the
                                 // collision
                  IDFlag_IsPickable,  // (bitmask)
                  0);                 // Check all nodes

      // If this returns something, we must be in contact, so find distance
      // between intersection and pointPosition
      if (selectedSceneNode &&
          strcmp(selectedSceneNode->getName(), "LandObject") == 0) {
        irr::f32 collisionDistance =
            pointPosition.getDistanceFrom(intersection);

        // If we're more collided with an object than the terrain, use this
        if (collisionDistance > localIntersection) {
          localIntersection = collisionDistance;
        }
      }

      // Also check for buoy collision
      if (selectedSceneNode &&
          strcmp(selectedSceneNode->getName(), "Buoy") == 0) {
        buoyCollision = true;
      }

      // And for other ship collision
      if (selectedSceneNode &&
          strcmp(selectedSceneNode->getName(), "OtherShip") == 0) {
        otherShipCollision = true;
      }

      // Contact model (proof of principle!)
      if (localIntersection > 1) {
        localIntersection = 1;  // Limit
      }

      if (localIntersection > 0) {
        // Simple 'proof of principle' values initially
        // reaction += localIntersection*100*maxForce * sign(axialSpd,0.1);
        // lateralReaction += localIntersection*100*maxForce *
        // sign(lateralSpd,0.1); turnReaction += localIntersection*100*maxForce
        // * sign(rateOfTurn,0.1);

        // Local speed at this point (TODO, include y component from pitch and
        // roll?)
        irr::core::vector3df localSpeedVector;
        localSpeedVector.X =
            lateralSpd + rateOfTurn * contactPoints.at(i).position.Z;
        localSpeedVector.Y = 0;
        localSpeedVector.Z =
            axialSpd - rateOfTurn * contactPoints.at(i).position.X;
        irr::core::vector3df normalLocalSpeedVector = localSpeedVector;
        normalLocalSpeedVector.normalize();
        irr::f32 frictionTorqueFactor =
            (contactPoints.at(i).position.crossProduct(normalLocalSpeedVector))
                .Y;  // Effect of unit friction force on ship's turning

        // Simple 'stiffness' based response
        irr::f32 reactionForce = localIntersection * 50 * maxForce;

        turnReaction += reactionForce * contactPoints.at(i).torqueEffect;
        reaction += reactionForce * contactPoints.at(i).normal.Z;
        lateralReaction += reactionForce * contactPoints.at(i).normal.X;

        // Friction response
        irr::f32 frictionCoeff = 0.5;
        turnReaction += reactionForce * frictionCoeff * frictionTorqueFactor;
        reaction += reactionForce * frictionCoeff * normalLocalSpeedVector.Z;
        lateralReaction +=
            reactionForce * frictionCoeff * normalLocalSpeedVector.X;

        // Damping
        // Project localSpeedVector onto contact normal. Damping reaction force
        // is proportional to this, and can be applied like the main reaction
        // force
        irr::f32 normalSpeed =
            localSpeedVector.dotProduct(contactPoints.at(i).normal);
        irr::f32 dampingForce =
            normalSpeed * 0.1 *
            maxForce;  // TODO - tune or make this configurable?
        turnReaction += dampingForce * contactPoints.at(i).torqueEffect;
        reaction += dampingForce * contactPoints.at(i).normal.Z;
        lateralReaction += dampingForce * contactPoints.at(i).normal.X;
      }

      // Debugging, show points:
      // contactDebugPoints.at(i*2)->setPosition(pointPosition);
      // contactDebugPoints.at(i*2 + 1)->setPosition(internalPointPosition);
    }
  }
}

irr::f32 OwnShip::getAngleCorrection() const { return angleCorrection; }

bool OwnShip::hasGPS() const { return gps; }

bool OwnShip::hasDepthSounder() const { return depthSounder; }

bool OwnShip::hasBowThruster() const { return bowThrusterPresent; }

bool OwnShip::hasSternThruster() const { return sternThrusterPresent; }

bool OwnShip::hasTurnIndicator() const { return turnIndicatorPresent; }

irr::f32 OwnShip::getMaxSounderDepth() const { return maxSounderDepth; }

std::vector<irr::core::vector3df> OwnShip::getCameraViews() const {
  return views;
}

std::vector<bool> OwnShip::getCameraIsHighView() const { return isHighView; }

void OwnShip::setViewVisibility(irr::u32 view) {
  if (is360textureShip) {
    irr::scene::ISceneNodeList childList = ship->getChildren();
    irr::scene::ISceneNodeList::ConstIterator it = childList.begin();
    int i = 0;
    while (it != childList.end()) {
      if (i == view) {
        (*it)->setVisible(true);
      } else {
        (*it)->setVisible(false);
      }

      i++;
      it++;
    }
  }
}

std::string OwnShip::getRadarConfigFile() const { return radarConfigFile; }

irr::f32 OwnShip::sign(irr::f32 inValue) const {
  if (inValue > 0) {
    return 1.0;
  }
  if (inValue < 0) {
    return -1.0;
  }
  return 0.0;
}

irr::f32 OwnShip::sign(irr::f32 inValue, irr::f32 threshold) const {
  if (threshold <= 0) {
    return sign(inValue);
  }

  if (inValue > threshold) {
    return 1.0;
  }
  if (inValue < -1 * threshold) {
    return -1.0;
  }
  return inValue / threshold;
}
