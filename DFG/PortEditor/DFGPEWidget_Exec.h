//
// Copyright (c) 2010-2016, Fabric Software Inc. All rights reserved.
//

#ifndef _DFGPEWidget_Exec_h
#define _DFGPEWidget_Exec_h

#include <FabricCore.h>
#include <FTL/StrRef.h>
#include <QtGui/QFrame>
#include <QtGui/QIcon>

class QLineEdit;
class QPushButton;
class QTabWidget;
class QVBoxLayout;

namespace FabricUI {
namespace DFG {

class DFGExecNotifier;
class DFGPEWidget_PortsContainer;
class DFGWidget;

class DFGPEWidget_Exec : public QFrame
{
  Q_OBJECT

public:

  DFGPEWidget_Exec(
    DFGWidget *dfgWidget,
    FabricCore::DFGBinding binding,
    FTL::StrRef execPath,
    FabricCore::DFGExec exec,
    QWidget *parent = NULL
    );

  ~DFGPEWidget_Exec();

  void setExec(
    FabricCore::DFGBinding binding,
    FTL::StrRef execPath,
    FabricCore::DFGExec exec
    );

protected slots:

  void onAddBlockButtonClicked();

  void onExecBlockInserted(
    unsigned blockIndex,
    FTL::CStrRef blockName
    );

  void onExecBlockRemoved(
    unsigned blockIndex,
    FTL::CStrRef blockName
    );

private:

  DFGWidget *m_dfgWidget;
  FabricCore::DFGBinding m_binding;
  std::string m_execPath;
  QString m_execPathQS;
  FabricCore::DFGExec m_exec;
  QSharedPointer<DFG::DFGExecNotifier> m_execNotifier;
  QIcon m_plusIcon;
  QTabWidget *m_tabWidget;
  QLineEdit *m_addBlockLineEdit;
  QPushButton *m_addBlockButton;
  QVBoxLayout *m_layout;
};

} // namespace DFG
} // namespace FabricUI

#endif // _DFGPEWidget_Exec_h
