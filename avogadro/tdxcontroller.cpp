/******************************************************************************
  This source file is part of the Avogadro project.
  This source code is released under the 3-Clause BSD License, (see "LICENSE").
******************************************************************************/

#include "tdxcontroller.h"
#include <SpaceMouse/CCategory.hpp>
#include <SpaceMouse/CCommand.hpp>
#include <SpaceMouse/CCommandSet.hpp>

#include <avogadro/rendering/glrenderer.h>
#include <avogadro/rendering/camera.h>
#include <avogadro/qtgui/molecule.h>
#include <avogadro/qtgui/toolplugin.h>
#include <avogadro/qtopengl/glwidget.h>

#include <QtWidgets/QAction>
#include <QtCore/QCoreApplication>
#include <qbytearray.h>>
#include <qbuffer.h>

#include <filesystem>

Avogadro::TDxController::TDxController(
  MainWindow* const mainWindow,
  Avogadro::QtOpenGL::GLWidget* const pGLWidget,
  QtGui::Molecule** ppMolecule)
  : CNavigation3D(false, true)
  , QObject(mainWindow)
  , m_pRootNode(std::make_shared<ActionTreeNode>("User Interface"))
  , m_pGLWidget(pGLWidget)
  , m_ppMolecule(ppMolecule)
  , m_pGLRenderer(&pGLWidget->renderer())
  , m_eyePosition({ 0.0, 0.0, 0.0 })
  , m_lookDirection({ 0.0, 0.0, 0.0 })
  , m_hitTestRadius(0.0)
{
  if (rayCount > 0) {
    m_rayOrigins[0].x = 0.0;
    m_rayOrigins[0].y = 0.0;
  }

  for (uint32_t i = 1; i < rayCount; i++) {
    float coefficient =
      sqrt(static_cast<float>(i) / static_cast<float>(rayCount));
    float angle = 2.4f * static_cast<float>(i);
    float x = coefficient * sin(angle);
    float y = coefficient * cos(angle);
    m_rayOrigins[i].x = x;
    m_rayOrigins[i].y = y;
  }
    
  QString pivotImagePath = QCoreApplication::applicationDirPath();

#ifdef WIN32
  pivotImagePath.append("/img/3dx_pivot.png");
#else
  pivotImagePath.append("/../Resources/3dx_pivot.png");
#endif

  m_pivotImage = QImage(pivotImagePath);

  if (!m_pivotImage.isNull() && m_pGLRenderer != nullptr) {
    m_pGLRenderer->m_iconData = reinterpret_cast<void*>(m_pivotImage.bits());
    m_pGLRenderer->m_iconWidth = m_pivotImage.width();
    m_pGLRenderer->m_iconHeight = m_pivotImage.height();
  }
}

void Avogadro::TDxController::enableController()
{
  PutProfileHint("Avogadro2");

  EnableNavigation(true, errorCode);

  if (errorCode)
    return;

  PutFrameTimingSource(TimingSource::SpaceMouse);

  return;
}

void Avogadro::TDxController::exportCommands(
  const QMap<QString, QList<QAction*>> &actionsMap)
{
  if (errorCode)
    return;

  TDx::SpaceMouse::CCommandSet commandSet("Default", "Main");

  for (auto& key : actionsMap.keys())
    addActions(key.toStdString(), m_pRootNode, actionsMap[key]);

  std::string pathCode;

  for (uint32_t i = 0; i < m_pRootNode->m_children.size(); i++) {
    pathCode.append(std::to_string(i));
    pathCode.append(".");
	commandSet.push_back(getCategory(pathCode, m_pRootNode->m_children[i]));
    pathCode.clear();
  }

  AddCommandSet(commandSet);
  PutActiveCommands("Default");

#ifdef WIN32
  AddImages(m_utilityIcons);
#endif
}

void Avogadro::TDxController::disableController()
{
  EnableNavigation(false, errorCode);
  return;
}

void Avogadro::TDxController::addActions(
  const std::string &path,
  const std::shared_ptr<ActionTreeNode> &pNode,
  const QList<QAction*> &actions)
{

  if (path.empty()) {
    std::copy(actions.begin(),
			  actions.end(),
              std::back_inserter(pNode->m_actions));
    return;
  }

  std::string childName;
  std::string remainingPath;

  for (auto itr = path.begin(); itr != path.end(); itr++) {
    if (*itr != '&' && *itr != '|')
      childName.push_back(*itr);

    if (*itr == '|') {
      std::copy(itr + 1, path.end(), std::back_inserter(remainingPath));
      break;
    }
  }

  for (auto& pNode : pNode->m_children) {
    if (pNode->m_nodeName == childName) {
      addActions(remainingPath, pNode, actions);
      return;
    }
  }

  auto pNewChild = std::make_shared<ActionTreeNode>(childName);
  pNode->m_children.push_back(pNewChild);

  addActions(remainingPath, pNewChild, actions);

  return;
}

QAction *Avogadro::TDxController::decodeAction(
  const std::string &pathCode,
  const std::shared_ptr<ActionTreeNode> &pNode) const
{
  std::string indexString;
  std::string remainingPath;

  for (auto itr = pathCode.begin(); itr != pathCode.end(); itr++) {
    if (*itr != '.') {
      indexString.push_back(*itr);
	}
    else {
      std::copy(itr + 1, pathCode.end(), std::back_inserter(remainingPath));
      break;
    }
  }

  uint32_t index = std::stoi(indexString);

  if (index < 0)
    return nullptr;

  if (remainingPath.empty()) {
    if (index >= pNode->m_actions.size())
      return nullptr;
    return pNode->m_actions[index];
  }

  if (index >= pNode->m_children.size())
    return nullptr;

  return decodeAction(remainingPath, pNode->m_children[index]);
}

// Inherited via CNavigation3D
// Getters

long Avogadro::TDxController::GetCameraMatrix(navlib::matrix_t &matrix) const
{
  if (m_pGLRenderer == nullptr)
    return navlib::make_result_code(navlib::navlib_errc::no_data_available);

  Eigen::Matrix4f _cameraMatrix =
    m_pGLRenderer->camera().modelView().matrix().inverse();

  matrix.m00 = _cameraMatrix(0, 0);
  matrix.m10 = _cameraMatrix(1, 0);
  matrix.m20 = _cameraMatrix(2, 0);
  matrix.m30 = _cameraMatrix(3, 0);

  matrix.m01 = _cameraMatrix(0, 1);
  matrix.m11 = _cameraMatrix(1, 1);
  matrix.m21 = _cameraMatrix(2, 1);
  matrix.m31 = _cameraMatrix(3, 1);

  matrix.m02 = _cameraMatrix(0, 2);
  matrix.m12 = _cameraMatrix(1, 2);
  matrix.m22 = _cameraMatrix(2, 2);
  matrix.m32 = _cameraMatrix(3, 2);

  matrix.m03 = _cameraMatrix(0, 3);
  matrix.m13 = _cameraMatrix(1, 3);
  matrix.m23 = _cameraMatrix(2, 3);
  matrix.m33 = _cameraMatrix(3, 3);

  return 0;
}

long Avogadro::TDxController::GetPointerPosition(
  navlib::point_t &position) const
{
  if (m_pGLRenderer == nullptr || m_pGLWidget == nullptr)
    return navlib::make_result_code(navlib::navlib_errc::no_data_available);

  QPoint pointerPos = m_pGLWidget->cursor().pos();
  pointerPos = m_pGLWidget->mapFromGlobal(pointerPos);

  Eigen::Vector3f worldPos = m_pGLRenderer->camera().unProject(
    Eigen::Vector2f(pointerPos.x(), pointerPos.y()));

  position.x = worldPos.x();
  position.y = worldPos.y();
  position.z = worldPos.z();

  return 0;
}

long Avogadro::TDxController::GetViewExtents(navlib::box_t &extents) const
{
  if (m_pGLRenderer == nullptr)
    return navlib::make_result_code(navlib::navlib_errc::no_data_available);

  extents.min.x = m_pGLRenderer->m_orthographicFrustum[0];
  extents.min.y = m_pGLRenderer->m_orthographicFrustum[2];
  extents.min.z = -m_pGLRenderer->m_orthographicFrustum[5];
  extents.max.x = m_pGLRenderer->m_orthographicFrustum[1];
  extents.max.y = m_pGLRenderer->m_orthographicFrustum[3];
  extents.max.z = -m_pGLRenderer->m_orthographicFrustum[4];
  return 0;
}

long Avogadro::TDxController::GetViewFOV(double &fov) const
{
  return navlib::make_result_code(navlib::navlib_errc::no_data_available);
}

long Avogadro::TDxController::GetViewFrustum(navlib::frustum_t &frustum) const
{
  if (m_pGLRenderer == nullptr)
    return navlib::make_result_code(navlib::navlib_errc::no_data_available);

  frustum.left = m_pGLRenderer->m_perspectiveFrustum[0];
  frustum.right = m_pGLRenderer->m_perspectiveFrustum[1];
  frustum.bottom = m_pGLRenderer->m_perspectiveFrustum[2];
  frustum.top = m_pGLRenderer->m_perspectiveFrustum[3];
  frustum.nearVal = m_pGLRenderer->m_perspectiveFrustum[4];
  frustum.farVal = m_pGLRenderer->m_perspectiveFrustum[5];
  return 0;
}
 
long Avogadro::TDxController::GetIsViewPerspective(
  navlib::bool_t &perspective) const
{
  if (m_pGLRenderer == nullptr)
    return navlib::make_result_code(navlib::navlib_errc::no_data_available);

  perspective = (m_pGLRenderer->camera().projectionType() ==
                 Avogadro::Rendering::Projection::Perspective);
  return 0;
}


TDx::SpaceMouse::CCategory Avogadro::TDxController::getCategory(
  const std::string &pathCode,
  const std::shared_ptr<ActionTreeNode> &pNode)
{

  TDx::SpaceMouse::CCategory result(pNode->m_nodeName, pNode->m_nodeName);
  std::string nextPathCode(pathCode);

  for (uint32_t i = 0u; i < pNode->m_children.size(); i++) {
    nextPathCode.append(std::to_string(i));
    nextPathCode.push_back('.');
    result.push_back(getCategory(nextPathCode, pNode->m_children[i]));
    nextPathCode = pathCode;
  }

  for (uint32_t i = 0u; i < pNode->m_actions.size(); i++) {
    
	if (pNode->m_actions[i]->iconText().isEmpty())
      continue;

	std::string commandId(nextPathCode + std::to_string(i));

    result.push_back(TDx::SpaceMouse::CCommand(
        commandId,
		pNode->m_actions[i]->iconText().toStdString(),
		pNode->m_actions[i]->toolTip().toStdString()));
        
#ifdef WIN32

	const QIcon iconImg = pNode->m_actions[i]->icon();
    const QImage qimage = iconImg.pixmap(QSize(256, 256)).toImage();
    QByteArray qbyteArray;
    QBuffer qbuffer(&qbyteArray);
    qimage.save(&qbuffer, "PNG");

    TDx::CImage icon = TDx::CImage::FromData(qbyteArray.toStdString(), 0, commandId.c_str());

    m_utilityIcons.push_back(icon);
#endif
  }
  return result;
}

long Avogadro::TDxController::GetModelExtents(navlib::box_t &extents) const
{
  if (m_pGLRenderer == nullptr)
    return navlib::make_result_code(navlib::navlib_errc::no_data_available);

  std::vector<bool> flags;

  m_pGLRenderer->scene().getBoundingBox(extents.min.x, 
									  extents.min.y,
                                      extents.min.z,
									  extents.max.x,
                                      extents.max.y,
									  extents.max.z, 
									  flags);

  return 0;
}

long Avogadro::TDxController::GetSelectionExtents(navlib::box_t &extents) const
{
  if (m_pGLRenderer == nullptr || *m_ppMolecule == nullptr)
    return navlib::make_result_code(navlib::navlib_errc::no_data_available);

  std::vector<bool> flags((*m_ppMolecule)->atomCount());

  for (uint32_t i = 0u; i < (*m_ppMolecule)->atomCount(); i++)
    flags[i] = (*m_ppMolecule)->atomSelected(i);

  m_pGLRenderer->scene().getBoundingBox(extents.min.x,
									  extents.min.y,
                                      extents.min.z, 
									  extents.max.x,
                                      extents.max.y,
									  extents.max.z,
									  flags);

  return 0;
}

long Avogadro::TDxController::GetSelectionTransform(
  navlib::matrix_t& transform) const
{
  return navlib::make_result_code(navlib::navlib_errc::no_data_available);
}

long Avogadro::TDxController::GetIsSelectionEmpty(navlib::bool_t &empty) const
{
  if (*m_ppMolecule == nullptr)
    return navlib::make_result_code(navlib::navlib_errc::no_data_available);

  for (uint32_t i = 0u; i < (*m_ppMolecule)->atomCount(); i++) {
    if ((*m_ppMolecule)->atomSelected(i)) {
      empty = false;
      return 0;
    }
  }
  empty = true;
  return 0;
} 

long Avogadro::TDxController::GetPivotPosition(navlib::point_t &position) const
{
  return navlib::make_result_code(navlib::navlib_errc::no_data_available);
}

long Avogadro::TDxController::GetPivotVisible(navlib::bool_t &visible) const
{
  if (m_pGLRenderer == nullptr)
    return navlib::make_result_code(navlib::navlib_errc::no_data_available);

  visible = m_pGLRenderer->m_drawIcon;
  return 0;
}

long Avogadro::TDxController::GetHitLookAt(navlib::point_t &position) const
{
  if (m_pGLRenderer == nullptr)
    return navlib::make_result_code(navlib::navlib_errc::no_data_available);

  constexpr float divThreshold = 0.01f;
  const float rayLength =
    2.0f * m_pGLRenderer->camera().distance(m_pGLRenderer->scene().center()) +
    m_pGLRenderer->scene().radius();
  const Eigen::Matrix4f transform =
    m_pGLRenderer->camera().modelView().matrix().inverse();
  Eigen::Vector4f origin = Eigen::Vector4f::Zero();
  float distance = std::numeric_limits<float>::max();

  if (m_hitTestRadius < divThreshold) {
    origin = transform * Eigen::Vector4f(0., 0., 0., 1.);
    origin /= origin.w();

    distance = m_pGLRenderer->scene().getHitDistance(
      Eigen::Vector3f(origin.x(), origin.y(), origin.z()),
      Eigen::Vector3f(m_lookDirection.x, m_lookDirection.y, m_lookDirection.z),
      rayLength);
  } else {

    Eigen::Vector4f originBuffer = Eigen::Vector4f::Zero();

    for (uint32_t i = 0; i < rayCount; i++) {

      float x = m_hitTestRadius * m_rayOrigins[i].x;
      float y = m_hitTestRadius * m_rayOrigins[i].y;

      originBuffer = transform * Eigen::Vector4f(x, y, 0., 1.);
      originBuffer /= originBuffer.w();

      float buffer = m_pGLRenderer->scene().getHitDistance(
        Eigen::Vector3f(originBuffer.x(), originBuffer.y(), originBuffer.z()),
        Eigen::Vector3f(m_lookDirection.x,
						m_lookDirection.y,
                        m_lookDirection.z),
		rayLength);

      if (buffer > 0.0f && buffer < distance) {
        origin = originBuffer;
        distance = buffer;
      }
    }
  }

  if (distance != std::numeric_limits<float>::max()) {
    position.x = origin.x() + distance * m_lookDirection.x;
    position.y = origin.y() + distance * m_lookDirection.y;
    position.z = origin.z() + distance * m_lookDirection.z;
    return 0;
  } else
    return navlib::make_result_code(navlib::navlib_errc::no_data_available);
}

// Setters 

long Avogadro::TDxController::SetCameraMatrix(const navlib::matrix_t &matrix)
{
  if (m_pGLRenderer == nullptr)
    return navlib::make_result_code(navlib::navlib_errc::no_data_available);

  Avogadro::Vector3f from(matrix.m03, matrix.m13, matrix.m23);
  Avogadro::Vector3f to =
    from - Avogadro::Vector3f(matrix.m02, matrix.m12, matrix.m22);
  Avogadro::Vector3f up(matrix.m01, matrix.m11, matrix.m21);

  m_pGLRenderer->camera().lookAt(from, to, up);

  return 0;
}

long Avogadro::TDxController::SetViewExtents(const navlib::box_t &extents)
{
  if (m_pGLRenderer == nullptr)
    return navlib::make_result_code(navlib::navlib_errc::no_data_available);

  m_pGLRenderer->m_orthographicFrustum[0] = extents.min.x;
  m_pGLRenderer->m_orthographicFrustum[1] = extents.max.x;
  m_pGLRenderer->m_orthographicFrustum[2] = extents.min.y;
  m_pGLRenderer->m_orthographicFrustum[3] = extents.max.y;
  m_pGLRenderer->m_orthographicFrustum[4] = -extents.max.z;
  m_pGLRenderer->m_orthographicFrustum[5] = -extents.min.z;

  return 0;
}

long Avogadro::TDxController::SetViewFOV(double fov)
{
  return navlib::make_result_code(navlib::navlib_errc::no_data_available);
}

long Avogadro::TDxController::SetViewFrustum(const navlib::frustum_t &frustum)
{
  return navlib::make_result_code(navlib::navlib_errc::no_data_available);
}

long Avogadro::TDxController::SetSelectionTransform(
  const navlib::matrix_t &matrix)
{
  return navlib::make_result_code(navlib::navlib_errc::no_data_available);
}

long Avogadro::TDxController::IsUserPivot(navlib::bool_t &userPivot) const
{
  userPivot = false;
  return 0;
}

long Avogadro::TDxController::SetPivotPosition(const navlib::point_t &position)
{
  if (m_pGLRenderer == nullptr)
    return navlib::make_result_code(navlib::navlib_errc::no_data_available);

  m_pGLRenderer->m_iconPosition.x() = position.x;
  m_pGLRenderer->m_iconPosition.y() = position.y;
  m_pGLRenderer->m_iconPosition.z() = position.z;
  return 0;
}

long Avogadro::TDxController::SetPivotVisible(bool visible)
{
  if (m_pGLWidget == nullptr || m_pGLRenderer == nullptr) 
    return navlib::make_result_code(navlib::navlib_errc::no_data_available);

  m_pGLRenderer->m_drawIcon = visible;
  m_pGLWidget->requestUpdate();
  return 0;
}

long Avogadro::TDxController::SetHitAperture(double aperture)
{
  m_hitTestRadius = aperture / 2.0;
  return 0;
}

long Avogadro::TDxController::SetHitDirection(const navlib::vector_t &direction)
{
  m_lookDirection = direction;
  return 0;
}

long Avogadro::TDxController::SetHitLookFrom(const navlib::point_t &eye)
{
  m_eyePosition = eye;
  return 0;
}

long Avogadro::TDxController::SetHitSelectionOnly(bool onlySelection)
{
  return navlib::make_result_code(navlib::navlib_errc::no_data_available);
}

long Avogadro::TDxController::SetActiveCommand(std::string commandId)
{
  QAction* pDecodedAction = decodeAction(commandId, m_pRootNode);

  if (pDecodedAction == nullptr)
    return navlib::make_result_code(navlib::navlib_errc::no_data_available);

  pDecodedAction->trigger();

  return 0;
}

long Avogadro::TDxController::SetTransaction(long value)
{
  if (m_pGLWidget == nullptr)
    return navlib::make_result_code(navlib::navlib_errc::no_data_available);

  m_pGLWidget->requestUpdate();

  return 0;
}
