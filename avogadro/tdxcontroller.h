/******************************************************************************
  This source file is part of the Avogadro project.
  This source code is released under the 3-Clause BSD License, (see "LICENSE").
******************************************************************************/

#ifdef _3DCONNEXION
#ifndef AVOGADRO_TDXCONTROLLER_H 
#define AVOGADRO_TDXCONTROLLER_H

#include <SpaceMouse/CImage.hpp>
#include <SpaceMouse/CNavigation3D.hpp>

#include <QtGui/QImage>
#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QString>
#include <QtCore/QList>
#include "mainwindow.h"
#include <array>

constexpr uint32_t rayCount = 50;

class QAction;

namespace Avogadro {

namespace Rendering {
class Camera;
class GLRenderer;
}

namespace QtOpenGL {
class GLWidget;
}

namespace QtGui {
class Molecule;
class ToolPlugin;
}


/**
 * This class is responsible for handling the TDx navigation in Avogadro2.
 */
class TDxController : private TDx::SpaceMouse::Navigation3D::CNavigation3D, private QObject
{


public:
  TDxController(MainWindow* const mainWindow,
                Avogadro::QtOpenGL::GLWidget* const pGLWidget,
                QtGui::Molecule** ppMolecule);
  /**
   * Enables the TDx navigation.
   */
  void enableController();

  /**
   * Disables the TDx navigation.
   */
  void disableController();

  /**
   * Exports interface utilities to the TDx wizard.
   * @param &actionsMap A map that contains pairs which constists of a string 
   * and an action list. The string represents a path through UI menus and 
   * submenus, to reach corresponding actions. Submenus names are expected 
   * to be separated by '|' char.
   */
  void exportCommands(const QMap<QString, QList<QAction*>> &actionsMap);
  
private:
  struct ActionTreeNode
  {
    std::string m_nodeName;
    std::vector<std::shared_ptr<ActionTreeNode>> m_children;
    std::vector<QAction*> m_actions;
    explicit ActionTreeNode(const std::string &nodeName)
      : m_nodeName(nodeName) {}
  };
  std::shared_ptr<ActionTreeNode> m_pRootNode;
  QtOpenGL::GLWidget* m_pGLWidget;
  Rendering::GLRenderer* m_pGLRenderer;
  QtGui::Molecule** m_ppMolecule;
  navlib::point_t m_eyePosition;
  navlib::vector_t m_lookDirection;
  QImage m_pivotImage;
#ifdef WIN32
  std::vector<TDx::CImage> m_utilityIcons;
#endif
  double m_hitTestRadius;
  std::array<navlib::point_t, rayCount> m_rayOrigins;
  std::error_code errorCode;

  /**
   * Adds a actions list to the action tree.
   * @param &path describes the recursive traversal through tree nodes
   * to the destination node. Nodes that does not exist will be created.
   * @param &pNode traversal starting point
   * @param &actions action list to add to the tree node
   */
  void addActions(const std::string &path,
                  const std::shared_ptr<ActionTreeNode> &pNode,
                  const QList<QAction*> &actions);

  /**
   * Returns CCategory hierarchy which reflects the actions tree.
   * Created CCommand's ID's are encoded paths to the actions in the tree.
   * @param &pNode root of the action tree
  */
  TDx::SpaceMouse::CCategory getCategory(
	const std::string &pathCode,
    const std::shared_ptr<ActionTreeNode> &pNode);

  /**
   * Recursively decodes a path from provided code and returns a QAction pointer
   * that has been reached in the actions tree. If the code is invalid, then nullptr
   * is returned.
   * @param &pathCode encoded path to the action in the actions tree
   * @param &pNode node from which the decoding begins
  */
  QAction *decodeAction(const std::string &pathCode,
                        const std::shared_ptr<ActionTreeNode> &pNode) const;

  // Inherited via CNavigation3D
  // Getters

  virtual long GetCameraMatrix(navlib::matrix_t& matrix) const override;
  virtual long GetPointerPosition(navlib::point_t& position) const override;
  virtual long GetViewExtents(navlib::box_t& extents) const override;
  virtual long GetViewFOV(double& fov) const override;
  virtual long GetViewFrustum(navlib::frustum_t& frustum) const override;
  virtual long GetIsViewPerspective(navlib::bool_t& perspective) const override;
  virtual long GetModelExtents(navlib::box_t& extents) const override;
  virtual long GetSelectionExtents(navlib::box_t& extents) const override;
  virtual long GetSelectionTransform(navlib::matrix_t& transform) const override;
  virtual long GetIsSelectionEmpty(navlib::bool_t& empty) const override;
  virtual long GetPivotPosition(navlib::point_t& position) const override;
  virtual long GetPivotVisible(navlib::bool_t& visible) const override;
  virtual long GetHitLookAt(navlib::point_t& position) const override;

  // Setters

  virtual long SetCameraMatrix(const navlib::matrix_t& matrix) override;
  virtual long SetViewExtents(const navlib::box_t& extents) override;
  virtual long SetViewFOV(double fov) override;
  virtual long SetViewFrustum(const navlib::frustum_t& frustum) override;
  virtual long SetSelectionTransform(const navlib::matrix_t& matrix) override;
  virtual long IsUserPivot(navlib::bool_t& userPivot) const override;
  virtual long SetPivotPosition(const navlib::point_t& position) override;
  virtual long SetPivotVisible(bool visible) override;
  virtual long SetHitAperture(double aperture) override;
  virtual long SetHitDirection(const navlib::vector_t& direction) override;
  virtual long SetHitLookFrom(const navlib::point_t& eye) override;
  virtual long SetHitSelectionOnly(bool onlySelection) override;
  virtual long SetActiveCommand(std::string commandId) override;
  virtual long SetTransaction(long value) override;
};
}

#endif // ! AVOGADRO_TDXCONTROLLER_H
#endif //  _3DCONNEXION