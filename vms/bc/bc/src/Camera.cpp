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

#include "Camera.hpp"
#include "Constants.hpp"

#include <cmath>


//using namespace irr;

Camera::Camera()
{

}

Camera::~Camera()
{
    //dtor
}


void Camera::load(irr::scene::ISceneManager* smgr, irr::ILogger* logger, irr::scene::ISceneNode* parent, std::vector<irr::core::vector3df> views, std::vector<bool> isHighView, irr::f32 hFOV, irr::f32 lookAngle, irr::f32 angleCorrection)
{
    this->hFOV = hFOV;
    camera = smgr->addCameraSceneNode(0, irr::core::vector3df(0,0,0), irr::core::vector3df(0,0,1));

    this->parent = parent;
    this->views = views;
    this->isHighView = isHighView;
    currentView = 0;
    this->lookAngle = lookAngle;
    minLookUpAngle = -85;
    maxLookUpAngle = 85;
    lookUpAngle = -20;
    this->angleCorrection = angleCorrection;

    this->logger = logger;

    verticalPanSpeed = 0;
    horizontalPanSpeed = 0;

    isHighViewActive = false;
    previousLookAngle = lookAngle;
    previousLookUpAngle = lookUpAngle;
}

irr::scene::ISceneNode* Camera::getSceneNode() const
{
    return camera;
}

irr::core::vector3df Camera::getPosition() const
{
    camera->updateAbsolutePosition();//ToDo: This may be needed, but seems odd that it's required
    return camera->getAbsolutePosition();
}

void Camera::lookUp()
{
    lookUpAngle++;
    if (lookUpAngle>maxLookUpAngle)
    {
        lookUpAngle=maxLookUpAngle;
    }
}

void Camera::lookDown()
{
    lookUpAngle--;
    if (lookUpAngle<minLookUpAngle)
    {
        lookUpAngle=minLookUpAngle;
    }
}

void Camera::setPanSpeed(irr::f32 horizontalPanSpeed){
    this->horizontalPanSpeed = horizontalPanSpeed;
}

void Camera::setVerticalPanSpeed(irr::f32 verticalPanSpeed){
    this->verticalPanSpeed = verticalPanSpeed;
}

void Camera::setLookUp(irr::f32 angle)
{
	lookUpAngle = angle;
}

void Camera::lookLeft()
{
    lookAngle--;
    while (lookAngle<0)
    {
        lookAngle+=360;
    }
}

void Camera::lookRight()
{
    lookAngle++;
    while (lookAngle>=360)
    {
        lookAngle-=360;
    }
}

void Camera::lookChange(irr::f32 deltaX, irr::f32 deltaY) //Change as a proportion of screen width
{
    lookAngle -= deltaX*hFOV*irr::core::RADTODEG;
    lookUpAngle += deltaY*hFOV*irr::core::RADTODEG; //hFOV for this, as both are scaled by screen width
    while (lookAngle<0)
    {
        lookAngle+=360;
    }
    while (lookAngle>=360)
    {
        lookAngle-=360;
    }
    if (lookUpAngle>maxLookUpAngle)
    {
        lookUpAngle=maxLookUpAngle;
    }
    if (lookUpAngle<minLookUpAngle)
    {
        lookUpAngle=minLookUpAngle;
    }
}

void Camera::lookStepLeft()
{
    lookAngle -= hFOV*irr::core::RADTODEG;
    while (lookAngle<0)
    {
        lookAngle+=360;
    }
}

void Camera::lookStepRight()
{
    lookAngle += hFOV*irr::core::RADTODEG;
    while (lookAngle>=360)
    {
        lookAngle-=360;
    }
}

void Camera::lookAhead()
{
    lookAngle = 0;
    lookUpAngle = 0;
}

void Camera::lookAstern()
{
    lookAngle = 180;
    lookUpAngle = 0;
}

void Camera::lookPort()
{
    lookAngle = 270;
    lookUpAngle = 0;
}

void Camera::lookStbd()
{
    lookAngle = 90;
    lookUpAngle = 0;
}

irr::f32 Camera::getLook() const
{
    return lookAngle;
}

irr::f32 Camera::getLookUp() const
{
    return lookUpAngle;
}

void Camera::moveForwards() 
{
    irr::core::vector3df frv(1.0f*std::sin(irr::core::DEGTORAD*(lookAngle-angleCorrection))*std::cos(irr::core::DEGTORAD*lookUpAngle), 1.0f*std::sin(irr::core::DEGTORAD*lookUpAngle), 1.0f*std::cos(irr::core::DEGTORAD*(lookAngle-angleCorrection))*std::cos(irr::core::DEGTORAD*lookUpAngle));
    views[currentView] += 0.05 * frv;
    
    //For displaying position
    std::string cameraPositionText = "Camera: (";
    cameraPositionText.append(irr::core::stringc(views[currentView].X).c_str());
    cameraPositionText.append(",");
    cameraPositionText.append(irr::core::stringc(views[currentView].Y).c_str());
    cameraPositionText.append(",");
    cameraPositionText.append(irr::core::stringc(views[currentView].Z).c_str());
    cameraPositionText.append(")");
    logger->log(cameraPositionText.c_str());
}

void Camera::moveBackwards() 
{
    irr::core::vector3df frv(1.0f*std::sin(irr::core::DEGTORAD*(lookAngle-angleCorrection))*std::cos(irr::core::DEGTORAD*lookUpAngle), 1.0f*std::sin(irr::core::DEGTORAD*lookUpAngle), 1.0f*std::cos(irr::core::DEGTORAD*(lookAngle-angleCorrection))*std::cos(irr::core::DEGTORAD*lookUpAngle));
    views[currentView] -= 0.05 * frv;

    //For displaying position
    std::string cameraPositionText = "Camera: (";
    cameraPositionText.append(irr::core::stringc(views[currentView].X).c_str());
    cameraPositionText.append(",");
    cameraPositionText.append(irr::core::stringc(views[currentView].Y).c_str());
    cameraPositionText.append(",");
    cameraPositionText.append(irr::core::stringc(views[currentView].Z).c_str());
    cameraPositionText.append(")");
    logger->log(cameraPositionText.c_str());
}

void Camera::highView(bool highViewRequired)
{
    if (isHighViewActive != highViewRequired) {
        if (!highViewRequired) {
            lookAngle = previousLookAngle;
            lookUpAngle = previousLookUpAngle;
            isHighViewActive = false;
        } else {
            previousLookAngle = lookAngle;
            previousLookUpAngle = lookUpAngle;
            lookAngle = 0;
            lookUpAngle = -89.99; //Almost straight down. Avoid -90 as this gives an odd rotation effect (gymbal lock?)
            isHighViewActive = true;
        }
    }
}

void Camera::changeView()
{
    currentView++;
    if (currentView==views.size()) {
        currentView = 0;
    }
    
    if (currentView<isHighView.size()) {
        if (isHighView[currentView]) {
            highView(true);
        } else {
            highView(false);
        }
    }

}

void Camera::setView(irr::u32 view) {
    if (view<views.size()) {
        currentView = view;
    }

    if (currentView<isHighView.size()) {
        if (isHighView[currentView]) {
            highView(true);
        } else {
            highView(false);
        }
    }

}

irr::u32 Camera::getView() const
{
    return currentView;
}

void Camera::setHFOV(irr::f32 hFOV)
{
    this->hFOV=hFOV;
    irr::f32 aspect=camera->getAspectRatio();

    irr::f32 vFOV = 2*atan(tan(hFOV/2)/aspect); //Calculate vertical field of view angle from horizontal one
    camera->setFOV(vFOV);
}

void Camera::updateViewport(irr::f32 aspect)
{
    camera->setAspectRatio(aspect);
    irr::f32 vFOV = 2*atan(tan(hFOV/2)/aspect); //Calculate vertical field of view angle from horizontal one
    camera->setFOV(vFOV);
}

void Camera::setActive()
{
    camera->getSceneManager()->setActiveCamera(camera);
}

void Camera::setNearValue(irr::f32 zn)
{
    camera->setNearValue(zn);
}

void Camera::setFarValue(irr::f32 zf)
{
    camera->setFarValue(zf);
}

void Camera::update(irr::f32 deltaTime)
{
     //link camera rotation to shipNode
        //Adjust camera angle if panning
        lookAngle += horizontalPanSpeed * deltaTime;
        while (lookAngle>=360) {
            lookAngle -= 360;
        }
        while (lookAngle<0) {
            lookAngle += 360;
        }
        
        lookUpAngle += verticalPanSpeed * deltaTime;
        if (lookUpAngle > maxLookUpAngle) {
            lookUpAngle = maxLookUpAngle;
        }
        if (lookUpAngle < minLookUpAngle) {
            lookUpAngle = minLookUpAngle;
        }

        // get transformation matrix of node
        irr::core::matrix4 m;
        m.setRotationDegrees(parent->getRotation());

        // transform forward vector of camera
        irr::core::vector3df frv(1.0f*std::sin(irr::core::DEGTORAD*(lookAngle-angleCorrection))*std::cos(irr::core::DEGTORAD*lookUpAngle), 1.0f*std::sin(irr::core::DEGTORAD*lookUpAngle), 1.0f*std::cos(irr::core::DEGTORAD*(lookAngle-angleCorrection))*std::cos(irr::core::DEGTORAD*lookUpAngle));
        m.transformVect(frv);

        // transform upvector of camera
        irr::core::vector3df upv(0.0f, 1.0f, 0.0f);
        m.transformVect(upv);

        // transform camera offset ('offset' is relative to the local ship coordinates, and stays the same.)
        //'offsetTransformed' is transformed into the global coordinates
        irr::core::vector3df offsetTransformed;
        m.transformVect(offsetTransformed,views[currentView]);

        //move camera and angle
        camera->setPosition(parent->getPosition() + offsetTransformed);
        camera->setUpVector(upv); //set up vector of camera
        camera->setTarget(parent->getPosition() + offsetTransformed + frv); //set target of camera (look at point)
        camera->updateAbsolutePosition();

        //also set rotation, so we can get camera's direction
        camera->setRotation(parent->getRotation());

}
