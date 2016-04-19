/*
 *  Copyright 2010-2016 Fabric Software Inc. All rights reserved.
 */

#ifndef __UI_SCENEHUB_CMD_HANDLER_H__
#define __UI_SCENEHUB_CMD_HANDLER_H__

#include "SHCmd.h"
#include <QtGui/QUndoStack>
#include "SHCmdRegistration.h"

namespace FabricUI {
namespace SceneHub {

class SHCmdHandler : public QObject {
  Q_OBJECT

  protected:
    class WrappedCmd;


  public:
    SHCmdHandler(FabricCore::Client client, QUndoStack *qUndoStack);

    virtual ~SHCmdHandler() {};
  

  public slots:
    void onSynchronizeCommands();


  protected:       
    unsigned int m_stackSize;
    FabricCore::Client m_client;
    QUndoStack *m_qUndoStack;
};

} // SceneHub
} // FabricUI 

#endif // __UI_SCENEHUB_CMD_HANDLER_H__