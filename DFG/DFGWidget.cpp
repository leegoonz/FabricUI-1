// Copyright 2010-2015 Fabric Software Inc. All rights reserved.
 
#include <assert.h>
#include <FabricCore.h>
#include <FabricUI/DFG/DFGMainWindow.h>
#include <FabricUI/DFG/DFGUICmdHandler.h>
#include <FabricUI/DFG/DFGWidget.h>
#include <FabricUI/DFG/Dialogs/DFGEditPortDialog.h>
#include <FabricUI/DFG/Dialogs/DFGGetStringDialog.h>
#include <FabricUI/DFG/Dialogs/DFGGetTextDialog.h>
#include <FabricUI/DFG/Dialogs/DFGNewVariableDialog.h>
#include <FabricUI/DFG/Dialogs/DFGPickVariableDialog.h>
#include <FabricUI/DFG/Dialogs/DFGSavePresetDialog.h>
#include <FabricUI/DFG/Dialogs/DFGNodePropertiesDialog.h>
#include <FabricUI/GraphView/NodeBubble.h>
#include <QtCore/QCoreApplication>
#include <QtCore/QDebug>
#include <QtCore/QUrl>
#include <QtGui/QColorDialog>
#include <QtGui/QCursor>
#include <QtGui/QDesktopServices>
#include <QtGui/QFileDialog>
#include <QtGui/QMessageBox>
 
using namespace FabricServices;
using namespace FabricUI;
using namespace FabricUI::DFG;

QSettings * DFGWidget::g_settings = NULL;

DFGWidget::DFGWidget(
  QWidget * parent, 
  FabricCore::Client &client,
  FabricCore::DFGHost &host,
  FabricCore::DFGBinding &binding,
  FTL::StrRef execPath,
  FabricCore::DFGExec &exec,
  FabricServices::ASTWrapper::KLASTManager * manager,
  DFGUICmdHandler *cmdHandler,
  const DFGConfig & dfgConfig,
  bool overTakeBindingNotifications
  )
  : QWidget( parent )
  , m_uiGraph( 0 )
  , m_router( 0 )
  , m_manager( manager )
  , m_dfgConfig( dfgConfig )
{
  m_uiController = new DFGController(
    0,
    this,
    client,
    m_manager,
    cmdHandler,
    overTakeBindingNotifications
    );

  QObject::connect(
    m_uiController.get(), SIGNAL( execChanged() ),
    this, SLOT( onExecChanged() )
    );

  m_uiHeader =
    new DFGExecHeaderWidget(
      this,
      m_uiController.get(),
      "Graph",
      dfgConfig.graphConfig
      );
  QObject::connect(
    this, SIGNAL( execChanged() ),
    m_uiHeader, SLOT( refresh() )
    );

  m_uiGraphViewWidget = new DFGGraphViewWidget(this, dfgConfig.graphConfig, NULL);

  m_klEditor =
    new DFGKLEditorWidget(
      this,
      m_uiController.get(),
      m_manager,
      m_dfgConfig
      );

  m_isEditable = true;
  if(binding.isValid())
  {
    FTL::StrRef editable = binding.getMetadata("editable");
    if(editable == "false")
      m_isEditable = false;
  }

  if(m_isEditable)
  {
    m_tabSearchWidget = new DFGTabSearchWidget(this, m_dfgConfig);
    m_tabSearchWidget->hide();
  }

  QVBoxLayout * layout = new QVBoxLayout();
  layout->setSpacing(0);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(m_uiHeader);
  layout->addWidget(m_uiGraphViewWidget);
  layout->addWidget(m_klEditor);
  layout->setContentsMargins(0, 0, 0, 0);
  setLayout(layout);
  setContentsMargins(0, 0, 0, 0);

  if(m_isEditable)
  {
    QObject::connect(
      m_uiHeader, SIGNAL(goUpPressed()),
      this, SLOT(onGoUpPressed())
      );

    QObject::connect(
      m_uiController.get(), SIGNAL(nodeEditRequested(FabricUI::GraphView::Node *)), 
      this, SLOT(onNodeEditRequested(FabricUI::GraphView::Node *))
      );
  }

  m_uiController->setHostBindingExec( host, binding, execPath, exec );


}

DFGWidget::~DFGWidget()
{
  if ( m_uiController )
    m_uiController->setRouter( 0 );

  if ( m_router )
    delete m_router;
}

GraphView::Graph * DFGWidget::getUIGraph()
{
  return m_uiGraph;
}

DFGController * DFGWidget::getUIController()
{
  return m_uiController.get();
}

DFGTabSearchWidget * DFGWidget::getTabSearchWidget()
{
  return m_tabSearchWidget;
}

DFGGraphViewWidget * DFGWidget::getGraphViewWidget()
{
  return m_uiGraphViewWidget;
}

DFGExecHeaderWidget * DFGWidget::getHeaderWidget()
{
  return m_uiHeader;
}

QMenu* DFGWidget::graphContextMenuCallback(FabricUI::GraphView::Graph* graph, void* userData)
{
  DFGWidget * graphWidget = (DFGWidget*)userData;
  if(graph->controller() == NULL)
    return NULL;
  QMenu* result = new QMenu(NULL);
  result->addAction("New empty graph");
  result->addAction("New empty function");
  result->addAction("New backdrop");

  const std::vector<GraphView::Node*> & nodes = graphWidget->getUIController()->graph()->selectedNodes();
  if(nodes.size() > 0)
  {
    result->addSeparator();
    result->addAction("Implode nodes");
  }

  result->addSeparator();
  result->addAction("New Variable");
  result->addAction("Read Variable (Get)");
  result->addAction("Write Variable (Set)");
  result->addAction("Cache Node");
  result->addSeparator();

  QAction * pasteAction = new QAction("Paste", graphWidget);
  pasteAction->setShortcut( QKeySequence(Qt::CTRL + Qt::Key_V) );
  // [Julien] When using shortcut in Qt, set the flag WidgetWithChildrenShortcut so the shortcut is specific to the widget
  pasteAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  result->addAction(pasteAction);

  result->addSeparator();

  QAction * resetZoomAction = new QAction("Reset Zoom", graphWidget);
  resetZoomAction->setShortcut( QKeySequence(Qt::CTRL + Qt::Key_0) );
  // [Julien] When using shortcut in Qt, set the flag WidgetWithChildrenShortcut so the shortcut is specific to the widget
  pasteAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  result->addAction(resetZoomAction);

  graphWidget->connect(result, SIGNAL(triggered(QAction*)), graphWidget, SLOT(onGraphAction(QAction*)));
  return result;
}

QMenu* DFGWidget::nodeContextMenuCallback(FabricUI::GraphView::Node* uiNode, void* userData)
{
  try
  {
    DFGWidget * graphWidget = (DFGWidget*)userData;
    FabricCore::DFGExec &exec = graphWidget->m_uiController->getExec();

    GraphView::Graph * graph = graphWidget->m_uiGraph;
    if(graph->controller() == NULL)
      return NULL;
    graphWidget->m_contextNode = uiNode;

    unsigned int varNodeCount = 0;
    unsigned int getNodeCount = 0;
    unsigned int setNodeCount = 0;
    unsigned int instNodeCount = 0;
    unsigned int userNodeCount = 0;

    const std::vector<GraphView::Node*> & nodes = graphWidget->getUIController()->graph()->selectedNodes();
    for(unsigned int i=0;i<nodes.size();i++)
    {
      if(nodes[i]->isBackDropNode())
      {
        userNodeCount++;
        continue;
      }

      char const * nodeName = nodes[i]->name().c_str();
      FabricCore::DFGNodeType dfgNodeType = exec.getNodeType( nodeName );
      
      if( dfgNodeType == FabricCore::DFGNodeType_Var)
        varNodeCount++;
      else if( dfgNodeType == FabricCore::DFGNodeType_Get)
        getNodeCount++;
      else if( dfgNodeType == FabricCore::DFGNodeType_Set)
        setNodeCount++;
      else if( dfgNodeType == FabricCore::DFGNodeType_Inst)
        instNodeCount++;
      else
        userNodeCount++;
    }

    bool onlyInstNodes = instNodeCount == nodes.size();
    bool someVarNodes = varNodeCount > 0;
    bool someGetNodes = getNodeCount > 0;
    bool someSetNodes = setNodeCount > 0;

    char const * nodeName = uiNode->name().c_str();

    QMenu* result = new QMenu(NULL);

    if(onlyInstNodes)
    {
      if(instNodeCount == 1)
      {
        QString uiDocUrl = exec.getNodeMetadata( nodeName, "uiDocUrl" );
        if(uiDocUrl.length() == 0)
        {
          FabricCore::DFGExec subExec = exec.getSubExec( nodeName );
          uiDocUrl = subExec.getMetadata( "uiDocUrl" );
        }
        if(uiDocUrl.length() > 0)
        {
          result->addAction("Documentation");
          result->addSeparator();
        }
      }

      result->addAction("Inspect - DoubleClick");
      result->addAction("Edit - Shift DoubleClick");
    }
     
    if(!someVarNodes && !someGetNodes && !someSetNodes)
    {
      result->addAction("Properties - F2");
    }

    result->addAction("Delete - Del");
    result->addSeparator();

    if(!someVarNodes)
    {
      result->addAction("Copy - Ctrl C");
    }

    result->addAction("Paste - Ctrl V");

    if(!someVarNodes)
    {
      result->addAction("Cut - Ctrl X");
    }

    if(onlyInstNodes)
    {
      if(instNodeCount == 1)
      {
        result->addSeparator();
        result->addAction("Save Preset");
        result->addAction("Export JSON");
      }

      result->addSeparator();
      result->addAction("Implode nodes");

      if(instNodeCount == 1)
      {
        FabricCore::DFGExec subExec = exec.getSubExec( nodeName );
        if(subExec.getType() == FabricCore::DFGExecType_Graph)
        {
          result->addAction("Explode node");
        }

        if(subExec.getExtDepCount() > 0)
        {
          result->addSeparator();
          result->addAction("Reload Extension(s)");
        }
      }
    }

    if(nodes.size() == 1)
    {
      result->addSeparator();
      result->addAction("Set Comment");
      result->addAction("Remove Comment");
    }

    graphWidget->connect(result, SIGNAL(triggered(QAction*)), graphWidget, SLOT(onNodeAction(QAction*)));
    return result;
  }
  catch(FabricCore::Exception e)
  {
    printf("Exception: %s\n", e.getDesc_cstr());
  }
  return NULL;
}

QMenu* DFGWidget::portContextMenuCallback(FabricUI::GraphView::Port* port, void* userData)
{
  DFGWidget * graphWidget = (DFGWidget*)userData;
  GraphView::Graph * graph = graphWidget->m_uiGraph;
  if(graph->controller() == NULL)
    return NULL;
  graphWidget->m_contextPort = port;
  QMenu* result = new QMenu(NULL);
  result->addAction("Edit");
  result->addAction("Rename");
  result->addAction("Delete");

  try
  {
    FabricCore::DFGExec &exec = graphWidget->m_uiController->getExec();
    if(exec.getExecPortCount() > 1)
    {
      result->addSeparator();

      result->addAction("Move top");
      result->addAction("Move up");
      result->addAction("Move down");
      result->addAction("Move bottom");
      result->addAction("Move inputs to end");
      result->addAction("Move outputs to end");
    }
  }
  catch(FabricCore::Exception e)
  {
    printf("Exception: %s\n", e.getDesc_cstr());
  }

  graphWidget->connect(result, SIGNAL(triggered(QAction*)), graphWidget, SLOT(onExecPortAction(QAction*)));
  return result;
}

QMenu* DFGWidget::sidePanelContextMenuCallback(FabricUI::GraphView::SidePanel* panel, void* userData)
{
  DFGWidget * graphWidget = (DFGWidget*)userData;
  GraphView::Graph * graph = graphWidget->m_uiGraph;
  if(graph->controller() == NULL)
    return NULL;
  graphWidget->m_contextSidePanel = panel;
  graphWidget->m_contextPortType = panel->portType();
  QMenu* result = new QMenu(NULL);
  result->addAction("Create Port");
  result->addSeparator();
  result->addAction("Scroll Up");
  result->addAction("Scroll Down");
  graphWidget->connect(result, SIGNAL(triggered(QAction*)), graphWidget, SLOT(onSidePanelAction(QAction*)));
  return result;
}

void DFGWidget::onGoUpPressed()
{
  FTL::StrRef execPath = m_uiController->getExecPath();
  if ( execPath.empty() )
    return;

  FTL::StrRef::Split split = execPath.rsplit('.');
  std::string parentExecPath = split.first;

  FabricCore::DFGBinding &binding = m_uiController->getBinding();
  FabricCore::DFGExec rootExec = binding.getExec();
  FabricCore::DFGExec parentExec =
    rootExec.getSubExec( parentExecPath.c_str() );

  maybeEditNode( parentExecPath, parentExec );
}

void DFGWidget::onGraphAction(QAction * action)
{
  QPointF mouseOffset(-40, -15);
  QPointF pos = m_uiGraphViewWidget->mapToScene(m_uiGraphViewWidget->mapFromGlobal(QCursor::pos()));
  pos = m_uiGraph->itemGroup()->mapFromScene(pos);
  pos += mouseOffset;

  if(action->text() == "New empty graph")
  {
    DFGGetStringDialog dialog(NULL, "graph", m_dfgConfig);
    if(dialog.exec() != QDialog::Accepted)
      return;

    QString text = dialog.text();
    if(text.length() == 0)
      return;

    m_uiController->cmdAddInstWithEmptyGraph(
      text.toUtf8().constData(),
      QPointF(pos.x(), pos.y())
      );
  }
  else if(action->text() == "New empty function")
  {
    DFGGetStringDialog dialog(NULL, "func", m_dfgConfig);
    if(dialog.exec() != QDialog::Accepted)
      return;

    QString text = dialog.text();
    if(text.length() == 0)
      return;

    m_uiController->beginInteraction();

    static const FTL::CStrRef initialCode = FTL_STR("\
dfgEntry {\n\
  // result = a + b;\n\
}\n");
    FTL::CStrRef nodeName =
      m_uiController->cmdAddInstWithEmptyFunc(
        text.toUtf8().constData(),
        initialCode,
        QPointF(pos.x(), pos.y())
        );
    GraphView::Node * uiNode = m_uiGraph->node(nodeName);
    if(uiNode)
    {
      maybeEditNode( uiNode );
    }
    m_uiController->endInteraction();
  }
  else if(action->text() == "New backdrop")
  {
    DFGGetStringDialog dialog(NULL, "backdrop", m_dfgConfig);
    if(dialog.exec() != QDialog::Accepted)
      return;

    QString text = dialog.text();
    if(text.length() == 0)
      return;

    m_uiController->cmdAddBackDrop(
      text.toUtf8().constData(),
      QPointF( pos.x(), pos.y() )
      );
  }
  else if(action->text() == "Implode nodes")
  {
    DFGGetStringDialog dialog(NULL, "graph", m_dfgConfig);
    // [Julien] FE-5188, FE-5276 Allows only alpha-numeric text only 
    // We do this because the nodes's name must be alpha-numerical only and not contains "-, +, ?,"
    dialog.alphaNumicStringOnly();
    if(dialog.exec() != QDialog::Accepted)
      return;

    QString text = dialog.text();
    if(text.length() == 0)
      return;

    const std::vector<GraphView::Node*> & nodes =
      m_uiController->graph()->selectedNodes();

    std::vector<FTL::StrRef> nodeNames;
    nodeNames.reserve( nodes.size() );
    for ( size_t i = 0; i < nodes.size(); ++i )
      nodeNames.push_back( nodes[i]->name() );

    std::string newNodeName =
      m_uiController->cmdImplodeNodes(
        nodeNames,
        text.toUtf8().constData()
        );

    m_uiGraph->clearSelection();
    if ( GraphView::Node *uiNode = m_uiGraph->node( newNodeName ) )
      uiNode->setSelected( true );
  }
  else if(action->text() == "New Variable")
  {
    DFGController *controller = getUIController();
    FabricCore::Client client = controller->getClient();
    FabricCore::DFGBinding &binding = controller->getBinding();
    FTL::CStrRef execPath = controller->getExecPath();

    DFGNewVariableDialog dialog( this, client, binding, execPath );
    if(dialog.exec() != QDialog::Accepted)
      return;

    std::string name = dialog.name().toUtf8().constData();
    if ( name.empty() )
      return;
    std::string dataType = dialog.dataType().toUtf8().constData();
    std::string extension = dialog.extension().toUtf8().constData();

    m_uiController->cmdAddVar(
      name,
      dataType,
      extension,
      pos
      );

    pos += QPointF(30, 30);
  }
  else if(action->text() == "Read Variable (Get)" ||
    action->text() == "Write Variable (Set)")
  {
    DFGController *controller = getUIController();
    FabricCore::Client client = controller->getClient();
    FabricCore::DFGBinding &binding = controller->getBinding();
    FTL::CStrRef execPath = controller->getExecPath();

    DFGPickVariableDialog dialog(NULL, client, binding, execPath);
    if(dialog.exec() != QDialog::Accepted)
      return;

    QString name = dialog.name();
    if(name.length() == 0)
      return;

    if(action->text() == "Read Variable (Get)")
    {
      m_uiController->cmdAddGet(
        "get",
        name.toUtf8().constData(),
        QPointF(pos.x(), pos.y())
        );
    }
    else
    {
      m_uiController->cmdAddSet(
        "set",
        name.toUtf8().constData(),
        QPointF(pos.x(), pos.y())
        );
    }
  }
  else if(action->text() == "Cache Node")
  {
    DFGController * controller = getUIController();
    controller->cmdAddInstFromPreset("Fabric.Core.Data.Cache", QPointF(pos.x(), pos.y()));
    pos += QPointF(30, 30);
  }
  else if(action->text() == "Reset Zoom")
  {
    onResetZoom();
  }
  else if(action->text() == "Paste - Ctrl V")
  {
    getUIController()->cmdPaste();
  }
}

void DFGWidget::onNodeAction(QAction * action)
{
  if(m_contextNode == NULL)
    return;

  char const * nodeName = m_contextNode->name().c_str();
  if(action->text() == "Documentation")
  {
    FabricCore::DFGExec &exec = m_uiController->getExec();
    QString uiDocUrl = exec.getNodeMetadata( nodeName, "uiDocUrl" );
    if(uiDocUrl.length() == 0 && exec.getNodeType(nodeName) == FabricCore::DFGNodeType_Inst)
    {
      FabricCore::DFGExec subExec = exec.getSubExec( nodeName );
      uiDocUrl = subExec.getMetadata( "uiDocUrl" );
    }
    if(uiDocUrl.length() > 0)
      QDesktopServices::openUrl(uiDocUrl);
  }
  else if(action->text() == "Inspect - DoubleClick")
  {
    emit nodeInspectRequested(m_contextNode);
  }
  else if(action->text() == "Edit - Shift DoubleClick")
  {
    maybeEditNode( m_contextNode );
  }
  else if(action->text() == "Delete - Del")
  {
    m_uiController->gvcDoRemoveNodes(m_contextNode);
  }
  else if(action->text() == "Copy - Ctrl C")
  {
    m_uiController->copy();
  }
  else if(action->text() == "Paste - Ctrl V")
  {
    m_uiController->cmdPaste();
  }
  else if(action->text() == "Cut - Ctrl X")
  {
    m_uiController->cmdCut();
  }
  else if(action->text() == "Export JSON")
  {
    FabricCore::DFGExec &exec = m_uiController->getExec();
    if ( exec.getNodeType(nodeName) != FabricCore::DFGNodeType_Inst )
      return;

    FabricCore::DFGExec subExec = exec.getSubExec( nodeName );

    QString title = subExec.getTitle();
    if(title.toLower().endsWith(".canvas"))
      title = title.left(title.length() - 7);

    FTL::CStrRef uiNodeColor = exec.getNodeMetadata( nodeName, "uiNodeColor" );
    if(!uiNodeColor.empty())
      subExec.setMetadata("uiNodeColor", uiNodeColor.c_str(), true, true);
    FTL::CStrRef uiHeaderColor = exec.getNodeMetadata( nodeName, "uiHeaderColor" );
    if(!uiHeaderColor.empty())
      subExec.setMetadata("uiHeaderColor", uiHeaderColor.c_str(), true, true);
    FTL::CStrRef uiTextColor = exec.getNodeMetadata( nodeName, "uiTextColor" );
    if(!uiTextColor.empty())
      subExec.setMetadata("uiTextColor", uiTextColor.c_str(), true, true);
    FTL::CStrRef uiTooltip = exec.getNodeMetadata( nodeName, "uiTooltip" );
    if(!uiTooltip.empty())
      subExec.setMetadata("uiTooltip", uiTooltip.c_str(), true, true);
    FTL::CStrRef uiDocUrl = exec.getNodeMetadata( nodeName, "uiDocUrl" );
    if(!uiDocUrl.empty())
      subExec.setMetadata("uiDocUrl", uiDocUrl.c_str(), true, true);
    FTL::CStrRef uiAlwaysShowDaisyChainPorts = exec.getNodeMetadata( nodeName, "uiAlwaysShowDaisyChainPorts" );
    if(!uiAlwaysShowDaisyChainPorts.empty())
      subExec.setMetadata("uiAlwaysShowDaisyChainPorts", uiAlwaysShowDaisyChainPorts.c_str(), true, true);

    QString lastPresetFolder = title;
    if(getSettings())
    {
      lastPresetFolder = getSettings()->value("DFGWidget/lastPresetFolder").toString();
      lastPresetFolder += "/" + title;
    }

    QString filter = "DFG Preset (*.canvas)";
    QString filePath = QFileDialog::getSaveFileName(this, "Export JSON", lastPresetFolder, filter, &filter);
    if(filePath.length() == 0)
      return;
    if(filePath.toLower().endsWith(".canvas.canvas"))
      filePath = filePath.left(filePath.length() - 7);

    if(getSettings())
    {
      QDir dir(filePath);
      dir.cdUp();
      getSettings()->setValue( "DFGWidget/lastPresetFolder", dir.path() );
    }

    std::string filePathStr = filePath.toUtf8().constData();

    try
    {
      // copy all defaults
      for(unsigned int i=0;i<subExec.getExecPortCount();i++)
      {
        std::string pinPath = nodeName;
        pinPath += ".";
        pinPath += subExec.getExecPortName(i);

        FTL::StrRef rType = exec.getNodePortResolvedType(pinPath.c_str());
        if(rType.size() == 0 || rType.find('$') >= 0)
          continue;
        if(rType.size() == 0 || rType.find('$') != rType.end())
          rType = subExec.getExecPortResolvedType(i);
        if(rType.size() == 0 || rType.find('$') != rType.end())
          rType = subExec.getExecPortTypeSpec(i);
        if(rType.size() == 0 || rType.find('$') != rType.end())
          continue;
        FabricCore::RTVal val =
          exec.getInstPortResolvedDefaultValue(pinPath.c_str(), rType.data());
        if(val.isValid())
          subExec.setPortDefaultValue(subExec.getExecPortName(i), val, false);
      }

      std::string json = subExec.exportJSON().getCString();
      FILE * file = fopen(filePathStr.c_str(), "wb");
      if(file)
      {
        fwrite(json.c_str(), json.length(), 1, file);
        fclose(file);
      }
    }
    catch(FabricCore::Exception e)
    {
      printf("Exception: %s\n", e.getDesc_cstr());
    }
  }
  else if(action->text() == "Save Preset")
  {
    FabricCore::DFGExec &exec = m_uiController->getExec();
    if ( exec.getNodeType( nodeName ) != FabricCore::DFGNodeType_Inst )
      return;

    try
    {
      // Get the title (name) of the preset
      FabricCore::DFGExec subExec = exec.getSubExec(nodeName);
      // QString title = subExec.getTitle();
      // Get the name of the last selected nodes instead
      const std::vector<GraphView::Node*> &nodes = m_uiController->graph()->selectedNodes();
      QString title = QString(nodes[nodes.size()-1]->name().c_str());
       
      FabricCore::DFGHost &host = m_uiController->getHost();

      DFGSavePresetDialog dialog( this, m_uiController.get(), title );
      // [Julien] FE-5188, FE-5276
      dialog.alphaNumicStringOnly();

      while(true)
      {
        if(dialog.exec() != QDialog::Accepted)
          return;

        QString name = dialog.name();
        // QString version = dialog.version();
        QString location = dialog.location();

        if(name.length() == 0 || location.length() == 0)
        {
          QMessageBox msg(QMessageBox::Warning, "Fabric Warning", 
            "You need to provide a valid name and pick a valid location!");
          msg.addButton("Ok", QMessageBox::AcceptRole);
          msg.exec();
          continue;
        }

        if(location.startsWith("Fabric.") || location.startsWith("Variables.") ||
          location == "Fabric" || location == "Variables")
        {
          QMessageBox msg(QMessageBox::Warning, "Fabric Warning", 
            "You can't save a preset into a factory path (below Fabric).");
          msg.addButton("Ok", QMessageBox::AcceptRole);
          msg.exec();
          continue;
        }

        std::string filePathStr = host.getPresetImportPathname(location.toUtf8().constData());
        filePathStr += "/";
        filePathStr += name.toUtf8().constData();
        filePathStr += ".canvas";

        FILE * file = fopen(filePathStr.c_str(), "rb");
        if(file)
        {
          fclose(file);
          file = NULL;

          QMessageBox msg(QMessageBox::Warning, "Fabric Warning", 
            "The file "+QString(filePathStr.c_str())+" already exists.\nAre you sure to overwrite the file?");
          msg.addButton("Cancel", QMessageBox::RejectRole);
          msg.addButton("Ok", QMessageBox::AcceptRole);
          if(msg.exec() != QDialog::Accepted)
            continue;
        }

        subExec.setTitle(name.toUtf8().constData());

        FTL::CStrRef uiNodeColor = exec.getNodeMetadata( nodeName, "uiNodeColor" );
        if(!uiNodeColor.empty())
          subExec.setMetadata("uiNodeColor", uiNodeColor.c_str(), true, true);
        FTL::CStrRef uiHeaderColor = exec.getNodeMetadata( nodeName, "uiHeaderColor" );
        if(!uiHeaderColor.empty())
          subExec.setMetadata("uiHeaderColor", uiHeaderColor.c_str(), true, true);
        FTL::CStrRef uiTextColor = exec.getNodeMetadata( nodeName, "uiTextColor" );
        if(!uiTextColor.empty())
          subExec.setMetadata("uiTextColor", uiTextColor.c_str(), true, true);
        FTL::CStrRef uiTooltip = exec.getNodeMetadata( nodeName, "uiTooltip" );
        if(!uiTooltip.empty())
          subExec.setMetadata("uiTooltip", uiTooltip.c_str(), true, true);
        FTL::CStrRef uiDocUrl = exec.getNodeMetadata( nodeName, "uiDocUrl" );
        if(!uiDocUrl.empty())
          subExec.setMetadata("uiDocUrl", uiDocUrl.c_str(), true, true);
        FTL::CStrRef uiAlwaysShowDaisyChainPorts = exec.getNodeMetadata( nodeName, "uiAlwaysShowDaisyChainPorts" );
        if(!uiAlwaysShowDaisyChainPorts.empty())
          subExec.setMetadata("uiAlwaysShowDaisyChainPorts", uiAlwaysShowDaisyChainPorts.c_str(), true, true);

        // copy all defaults
        for(unsigned int i=0;i<subExec.getExecPortCount();i++)
        {
          std::string pinPath = nodeName;
          pinPath += ".";
          pinPath += subExec.getExecPortName(i);

          FTL::StrRef rType = exec.getNodePortResolvedType(pinPath.c_str());
          if(rType.size() == 0 || rType.find('$') != rType.end())
            rType = subExec.getExecPortResolvedType(i);
          if(rType.size() == 0 || rType.find('$') != rType.end())
            rType = subExec.getExecPortTypeSpec(i);
          if(rType.size() == 0 || rType.find('$') != rType.end())
            continue;
          FabricCore::RTVal val =
            exec.getInstPortResolvedDefaultValue(pinPath.c_str(), rType.data());
          if(val.isValid())
            subExec.setPortDefaultValue(subExec.getExecPortName(i), val, false);
        }

        std::string json = subExec.exportJSON().getCString();
        file = fopen(filePathStr.c_str(), "wb");
        if(!file)
        {
          QMessageBox msg(QMessageBox::Warning, "Fabric Warning", 
            "The file "+QString(filePathStr.c_str())+" cannot be opened for writing.");
          msg.addButton("Ok", QMessageBox::AcceptRole);
          msg.exec();
          continue;
        }

        fwrite(json.c_str(), json.length(), 1, file);
        fclose(file);

        subExec.setImportPathname(filePathStr.c_str());
        subExec.attachPresetFile(
          location.toUtf8().constData(),
          name.toUtf8().constData(),
          true // replaceExisting
          );

        emit newPresetSaved(filePathStr.c_str());
        // update the preset search paths within the controller
        m_uiController->onVariablesChanged();
        break;
      }
    }
    catch(FabricCore::Exception e)
    {
      QMessageBox msg(QMessageBox::Warning, "Fabric Warning", 
        e.getDesc_cstr());
      msg.addButton("Ok", QMessageBox::AcceptRole);
      msg.exec();
      return;
    }
  }
  else if(action->text() == "Implode nodes")
  {
    DFGGetStringDialog dialog(NULL, "graph", m_dfgConfig);
    // [Julien] Allows only alpha-numeric text only 
    // We do this because the nodes's name must be alpha-numerical only
    // and not contains "-, +, ?,"
    dialog.alphaNumicStringOnly();
    if(dialog.exec() != QDialog::Accepted)
      return;

    QString text = dialog.text();
    if(text.length() == 0)
      return;

    const std::vector<GraphView::Node*> & nodes =
      m_uiController->graph()->selectedNodes();

    std::vector<FTL::StrRef> nodeNames;
    nodeNames.reserve( nodes.size() );
    for ( size_t i = 0; i < nodes.size(); ++i )
      nodeNames.push_back( nodes[i]->name() );

    std::string newNodeName =
      m_uiController->cmdImplodeNodes(
        nodeNames,
        text.toUtf8().constData()
        );

    m_uiGraph->clearSelection();
    if ( GraphView::Node *uiNode = m_uiGraph->node( newNodeName ) )
      uiNode->setSelected( true );
  }
  else if(action->text() == "Explode node")
  {
    std::vector<std::string> newNodeNames =
      m_uiController->cmdExplodeNode( nodeName );

    m_uiGraph->clearSelection();
    for ( std::vector<std::string>::const_iterator it = newNodeNames.begin();
      it != newNodeNames.end(); ++it )
    {
      if ( GraphView::Node *uiNode = m_uiGraph->node( *it ) )
        uiNode->setSelected( true );
    }
  }
  else if(action->text() == "Reload Extension(s)")
  {
    m_uiController->reloadExtensionDependencies(nodeName);
  }
  else if(action->text() == "Properties - F2")
  {
    editPropertiesForCurrentSelection();
  }
  else if(action->text() == "Set Comment")
  {
    onBubbleEditRequested(m_contextNode);
  }
  else if(action->text() == "Remove Comment")
  {
    m_uiController->setNodeCommentExpanded( m_contextNode->name(), false );
    m_uiController->cmdSetNodeComment(
      m_contextNode->name(), FTL::CStrRef()
      );
  }

  m_contextNode = NULL;
}

void DFGWidget::onNodeEditRequested(FabricUI::GraphView::Node * node)
{
  maybeEditNode( node );
}

void DFGWidget::onExecPortAction(QAction * action)
{
  if(m_contextPort == NULL)
    return;

  char const * portName = m_contextPort->name().c_str();
  if(action->text() == "Delete")
  {
    m_uiController->cmdRemovePort( portName );
  }
  else if ( action->text() == "Rename" )
  {
    DFGBaseDialog dialog( this );
    QLineEdit *lineEdit = new QLineEdit();
    lineEdit->setText( portName );
    lineEdit->selectAll();
    QObject::connect(
      lineEdit, SIGNAL(returnPressed()),
      &dialog, SLOT(accept())
      );
    dialog.addInput( lineEdit, "Desired name" );

    if ( dialog.exec() == QDialog::Accepted )
    {
      QString desiredNewName = lineEdit->text();
      m_uiController->cmdRenameExecPort(
        portName,
        desiredNewName.toUtf8().constData()
        );
    }
  }
  else if(action->text() == "Edit")
  {
    try
    {
      FabricCore::Client &client = m_uiController->getClient();
      FabricCore::DFGExec &exec = m_uiController->getExec();

      bool canEditPortType = m_uiController->isViewingRootGraph();
      DFGEditPortDialog dialog( this, client, false, canEditPortType, m_dfgConfig );

      dialog.setTitle(portName);
      bool canEditDataType = m_uiController->isViewingRootGraph();
      if ( canEditDataType )
        dialog.setDataType(exec.getExecPortResolvedType(portName));

      FTL::StrRef uiHidden = exec.getExecPortMetadata(portName, "uiHidden");
      if(uiHidden == "true")
        dialog.setHidden();
      FTL::StrRef uiOpaque = exec.getExecPortMetadata(portName, "uiOpaque");
      if(uiOpaque == "true")
        dialog.setOpaque();
      FTL::StrRef uiRange = exec.getExecPortMetadata(portName, "uiRange");
      if(uiRange.size() > 0)
      {
        QString filteredUiRange;
        for(unsigned int i=0;i<uiRange.size();i++)
        {
          char c = uiRange[i];
          if(isalnum(c) || c == '.' || c == ',' || c == '-')
            filteredUiRange += c;
        }

        QStringList parts = filteredUiRange.split(',');
        if(parts.length() == 2)
        {
          float minimum = parts[0].toFloat();
          float maximum = parts[1].toFloat();
          dialog.setHasRange(true);
          dialog.setRangeMin(minimum);
          dialog.setRangeMax(maximum);
        }
      }
      FTL::StrRef uiCombo = exec.getExecPortMetadata(portName, "uiCombo");
      std::string uiComboStr;
      if(uiCombo.size() > 0)
        uiComboStr = uiCombo.data();
      if(uiComboStr.size() > 0)
      {
        if(uiComboStr[0] == '(')
          uiComboStr = uiComboStr.substr(1);
        if(uiComboStr[uiComboStr.size()-1] == ')')
          uiComboStr = uiComboStr.substr(0, uiComboStr.size()-1);

        QStringList parts = QString(uiComboStr.c_str()).split(',');
        dialog.setHasCombo(true);
        dialog.setComboValues(parts);
      }

      emit portEditDialogCreated(&dialog);

      if(dialog.exec() != QDialog::Accepted)
        return;

      emit portEditDialogInvoked(&dialog, NULL);

      std::string newPortName = dialog.title().toUtf8().constData();

      std::string typeSpec = dialog.dataType().toUtf8().constData();

      std::string extDep = dialog.extension().toUtf8().constData();

      std::string uiMetadata;
      {
        FTL::JSONEnc<> metaDataEnc( uiMetadata );
        FTL::JSONObjectEnc<> metaDataObjectEnc( metaDataEnc );
        if(dialog.hidden())
        {
          FTL::JSONEnc<> enc( metaDataObjectEnc, FTL_STR("uiHidden") );
          FTL::JSONStringEnc<> valueEnc( enc, FTL_STR("true") );
        }
        if(dialog.opaque())
        {
          FTL::JSONEnc<> enc( metaDataObjectEnc, FTL_STR("uiOpaque") );
          FTL::JSONStringEnc<> valueEnc( enc, FTL_STR("true") );
        }
        if(dialog.hasRange())
        {
          QString range = "(" + QString::number(dialog.rangeMin()) + ", " + QString::number(dialog.rangeMax()) + ")";
          FTL::JSONEnc<> enc( metaDataObjectEnc, FTL_STR("uiRange") );
          std::string rangeStr = range.toUtf8().constData();
          FTL::CStrRef rangeRef = rangeStr;
          FTL::JSONStringEnc<> valueEnc( enc, rangeRef );
        }
        if(dialog.hasCombo())
        {
          QStringList combo = dialog.comboValues();
          QString flat = "(";
          for(int i=0;i<combo.length();i++)
          {
            if(i > 0)
              flat += ", ";
            flat += "\"" + combo[i] + "\"";
          }
          flat += ")";

          FTL::JSONEnc<> enc( metaDataObjectEnc, FTL_STR("uiCombo") );
          std::string flatStr = flat.toUtf8().constData();
          FTL::CStrRef flatRef = flatStr;
          FTL::JSONStringEnc<> valueEnc( enc, flatRef );
        }

        emit portEditDialogInvoked(&dialog, &metaDataObjectEnc);
      }

      if ( FTL::StrRef( uiMetadata ) == FTL_STR("{}") )
        uiMetadata.clear();

      m_uiController->cmdEditPort(
        portName,
        newPortName,
        typeSpec,
        extDep,
        uiMetadata
        );
    }
    catch(FabricCore::Exception e)
    {
      printf("Exception: %s\n", e.getDesc_cstr());
    }

    // QString dataType = dialog.dataType();
    // if(m_uiController->setArg(m_contextPort->name(), dataType))
    // {
    //   // setup the value editor
    // }
  }
  else if(action->text() == "Move top" ||
    action->text() == "Move up" ||
    action->text() == "Move down" ||
    action->text() == "Move bottom" ||
    action->text() == "Move inputs to end" || 
    action->text() == "Move outputs to end")
  {
    try
    {
      FabricCore::DFGBinding &binding = m_uiController->getBinding();
      FTL::CStrRef execPath = m_uiController->getExecPath();
      FabricCore::DFGExec &exec = m_uiController->getExec();

      // create an index list with the inputs first
      std::vector<unsigned int> inputsFirst;
      std::vector<unsigned int> outputsFirst;
      for(unsigned int i=0;i<exec.getExecPortCount();i++)
      {
        if(exec.getExecPortType(i) == FEC_DFGPortType_IO)
        {
          inputsFirst.push_back(i);
          outputsFirst.push_back(i);
        }
      }
      for(unsigned int i=0;i<exec.getExecPortCount();i++)
      {
        if(exec.getExecPortType(i) == FEC_DFGPortType_In)
          inputsFirst.push_back(i);
        else if(exec.getExecPortType(i) == FEC_DFGPortType_Out)
          outputsFirst.push_back(i);
      }
      for(unsigned int i=0;i<exec.getExecPortCount();i++)
      {
        if(exec.getExecPortType(i) == FEC_DFGPortType_In)
          outputsFirst.push_back(i);
        else if(exec.getExecPortType(i) == FEC_DFGPortType_Out)
          inputsFirst.push_back(i);
      }

      FTL::StrRef portNameRef = portName;
      FabricCore::DFGPortType portType = exec.getExecPortType(portNameRef.data());
      std::vector<unsigned int> indices;
      bool reorder = false;

      if(exec.getExecPortType(exec.getExecPortCount()-1) == FEC_DFGPortType_Out)
        indices = inputsFirst;
      else
        indices = outputsFirst;

      unsigned a = UINT_MAX;
      unsigned b = UINT_MAX;

      if(action->text() == "Move top")
      {
        for(size_t i=0;i<indices.size();i++)
        {
          if(b == UINT_MAX && exec.getExecPortType(indices[i]) == portType)
          {
            b = i;
          }
          if(portNameRef == exec.getExecPortName(indices[i]))
          {
            a = i;
            reorder = a > 0;
          }
        }

        if(a != b)
        {
          unsigned int temp = indices[a];
          for(unsigned i=a;i>b;i--)
            indices[i] = indices[i-1];
          indices[b] = temp;

          a = UINT_MAX;
          b = UINT_MAX;
        }
      }
      else if(action->text() == "Move up")
      {
        for(size_t i=0;i<indices.size();i++)
        {
          if(portNameRef == exec.getExecPortName(indices[i]))
          {
            a = i;
            b = a - 1;
            reorder = a > 0;
            break;
          }
        }
      }
      else if(action->text() == "Move down")
      {
        for(unsigned int i=0;i<indices.size();i++)
        {
          if(portNameRef == exec.getExecPortName(indices[i]))
          {
            a = i;
            b = a + 1;
            reorder = true;
            break;
          }
        }
      }
      else if(action->text() == "Move bottom")
      {
        for(size_t i=0;i<indices.size();i++)
        {
          if(portNameRef == exec.getExecPortName(indices[i]))
          {
            a = i;
          }
          if(exec.getExecPortType(indices[i]) == portType)
          {
            b = i;
          }
        }

        if(a != b)
        {
          unsigned int temp = indices[a];
          for(unsigned int i=a;i<b;i++)
            indices[i] = indices[i+1];
          indices[b] = temp;

          a = UINT_MAX;
          b = UINT_MAX;
          reorder = true;
        }
      }
      else if(action->text() == "Move inputs to end")
      {
        indices = outputsFirst;
        reorder = true;
      }
      else if(action->text() == "Move outputs to end")
      {
        indices = inputsFirst;
        reorder = true;
      }

      if(a != UINT_MAX && b != UINT_MAX && a != b)
      {
        // swap indices
        unsigned int temp = indices[a];
        indices[a] = indices[b];
        indices[b] = temp;
        reorder = true;
      }

      if(!reorder)
        return;

      if(indices.size() > 0)
        m_uiController->cmdReorderPorts(binding, execPath, exec, indices);
    }
    catch(FabricCore::Exception e)
    {
      printf("Exception: %s\n", e.getDesc_cstr());
    }
  }

  m_contextPort = NULL;
}

void DFGWidget::onSidePanelAction(QAction * action)
{
  if(action->text() == "Create Port")
  {
    FabricCore::Client &client = m_uiController->getClient();

    bool canEditPortType = m_uiController->isViewingRootGraph();
    DFGEditPortDialog dialog( this, client, true, canEditPortType, m_dfgConfig );
    // [Julien] FE-5188, FE-5276
    dialog.alphaNumicStringOnly();

    if(m_contextPortType == FabricUI::GraphView::PortType_Output)
      dialog.setPortType("In");
    else
      dialog.setPortType("Out");

    emit portEditDialogCreated(&dialog);

    if(dialog.exec() != QDialog::Accepted)
      return;

    std::string metaData;
    {
      FTL::JSONEnc<> metaDataEnc( metaData );
      FTL::JSONObjectEnc<> metaDataObjectEnc( metaDataEnc );
      if(dialog.hidden())
      {
        FTL::JSONEnc<> enc( metaDataObjectEnc, FTL_STR("uiHidden") );
        FTL::JSONStringEnc<> valueEnc( enc, FTL_STR("true") );
      }
      if(dialog.opaque())
      {
        FTL::JSONEnc<> enc( metaDataObjectEnc, FTL_STR("uiOpaque") );
        FTL::JSONStringEnc<> valueEnc( enc, FTL_STR("true") );
      }
      if(dialog.hasRange())
      {
        QString range = "(" + QString::number(dialog.rangeMin()) + ", " + QString::number(dialog.rangeMax()) + ")";
        FTL::JSONEnc<> enc( metaDataObjectEnc, FTL_STR("uiRange") );
        std::string rangeStr = range.toUtf8().constData();
        FTL::CStrRef rangeRef = rangeStr;
        FTL::JSONStringEnc<> valueEnc( enc, rangeRef );
      }
      if(dialog.hasCombo())
      {
        QStringList combo = dialog.comboValues();
        QString flat = "(";
        for(int i=0;i<combo.length();i++)
        {
          if(i > 0)
            flat += ", ";
          flat += "\"" + combo[i] + "\"";
        }
        flat += ")";

        FTL::JSONEnc<> enc( metaDataObjectEnc, FTL_STR("uiCombo") );
        std::string flatStr = flat.toUtf8().constData();
        FTL::CStrRef flatRef = flatStr;
        FTL::JSONStringEnc<> valueEnc( enc, flatRef );
      }

      emit portEditDialogInvoked(&dialog, &metaDataObjectEnc);
    }

    QString title = dialog.title();

    QString dataType = dialog.dataType();
    QString extension = dialog.extension();

    if(title.length() > 0)
    {
      FabricCore::DFGPortType portType = FabricCore::DFGPortType_Out;
      if ( dialog.portType() == "In" )
        portType = FabricCore::DFGPortType_In;
      else if ( dialog.portType() == "IO" )
        portType = FabricCore::DFGPortType_IO;

      if(metaData == "{}")
        metaData = "";

      m_uiController->cmdAddPort(
        title.toUtf8().constData(),
        portType, 
        dataType.toUtf8().constData(),
        FTL::CStrRef(), // portToConnect
        FTL::StrRef( extension.toUtf8().constData() ).trim(),
        metaData
        );
    }
  }
  else if(action->text() == "Scroll Up")
  {
    m_contextSidePanel->scroll(m_contextSidePanel->size().height());
  }
  else if(action->text() == "Scroll Down")
  {
    m_contextSidePanel->scroll(-m_contextSidePanel->size().height());
  }
}

void DFGWidget::onHotkeyPressed(Qt::Key key, Qt::KeyboardModifier mod, QString name)
{
  if(name == "PanGraph")
  {
    m_uiGraph->mainPanel()->setAlwaysPan(true);
  }
}

void DFGWidget::onHotkeyReleased(Qt::Key key, Qt::KeyboardModifier mod, QString name)
{
  if(name == "PanGraph")
  {
    m_uiGraph->mainPanel()->setAlwaysPan(false);
  }
}

void DFGWidget::onKeyPressed(QKeyEvent * event)
{
  if(getUIGraph() && !event->isAutoRepeat() && getUIGraph()->pressHotkey((Qt::Key)event->key(), (Qt::KeyboardModifier)(int)event->modifiers()))
    event->accept();
  else
    keyPressEvent(event);  
}

void DFGWidget::onKeyReleased(QKeyEvent * event)
{
  if(getUIGraph() && !event->isAutoRepeat() && getUIGraph()->releaseHotkey((Qt::Key)event->key(), (Qt::KeyboardModifier)(int)event->modifiers()))
    event->accept();
  else
    keyPressEvent(event);  
}

void DFGWidget::onBubbleEditRequested(FabricUI::GraphView::Node * node)
{
  QString text;
  bool visible = true;
  bool collapsed = false;

  GraphView::NodeBubble * bubble = node->bubble();
  if ( bubble )
  {
    text = bubble->text();
    visible = bubble->isVisible();
    collapsed = bubble->collapsed();
    bubble->show();
    bubble->expand();
  }

  DFGGetTextDialog dialog(NULL, text);
  if ( dialog.exec() == QDialog::Accepted )
  {
    m_uiController->cmdSetNodeComment(
      node->name(),
      dialog.text().toUtf8().constData()
      );
    m_uiController->setNodeCommentExpanded(
      node->name(),
      true
      );
  }
  else if ( bubble )
  {
    if ( collapsed )
      bubble->collapse();
    bubble->setVisible( visible );
  }
}

void DFGWidget::onCopy()
{
  getUIController()->copy();
}

void DFGWidget::onCut()
{
  getUIController()->cmdCut();
}

void DFGWidget::onPaste()
{
  getUIController()->cmdPaste();
}

void DFGWidget::onResetZoom()
{
  getUIController()->zoomCanvas(1.0);
}

void DFGWidget::onToggleDimConnections()
{
  m_uiGraph->config().dimConnectionLines = !m_uiGraph->config().dimConnectionLines;
  std::vector<GraphView::Connection *> connections = m_uiGraph->connections();
  for(size_t i=0;i<connections.size();i++)
    connections[i]->update();
  // [Julien] FE-5264 Sets the dimConnectionLines property to the settings
  if(getSettings()) getSettings()->setValue( "DFGWidget/dimConnectionLines", m_uiGraph->config().dimConnectionLines );
}

void DFGWidget::onTogglePortsCentered()
{
  m_uiGraph->config().portsCentered = !m_uiGraph->config().portsCentered;
  m_uiGraph->sidePanel(GraphView::PortType_Input)->updateItemGroupScroll();  
  m_uiGraph->sidePanel(GraphView::PortType_Output)->updateItemGroupScroll();  
  // [Julien] FE-5264 Sets the onTogglePortsCentered property to the settings
  if(getSettings()) getSettings()->setValue( "DFGWidget/portsCentered", m_uiGraph->config().portsCentered );
}

bool DFGWidget::maybeEditNode(
  FabricUI::GraphView::Node * node
  )
{
  if ( node->isBackDropNode() )
    return false;

  try
  {
    FTL::CStrRef nodeName = node->name();

    FabricCore::DFGExec &exec = m_uiController->getExec();
    if ( exec.getNodeType( nodeName.c_str() )
      != FabricCore::DFGNodeType_Inst )
      return false;

    std::string subExecPath = m_uiController->getExecPath();
    if ( !subExecPath.empty() )
      subExecPath += '.';
    subExecPath += nodeName;

    FabricCore::DFGExec subExec = exec.getSubExec( nodeName.c_str() );
    return maybeEditNode( subExecPath, subExec );
  }
  catch(FabricCore::Exception e)
  {
    printf("Exception: %s\n", e.getDesc_cstr());
  }
  return false;
}

bool DFGWidget::maybeEditNode(
  FTL::StrRef execPath,
  FabricCore::DFGExec &exec
  )
{
  try
  {
    if(m_klEditor->isVisible() && m_klEditor->hasUnsavedChanges())
    {
      QMessageBox msg(QMessageBox::Warning, "Fabric Warning", 
        "You haven't saved the code.\nYou are going to lose the changes.\nSure?");

      msg.addButton("Save Now", QMessageBox::AcceptRole);
      msg.addButton("Ok", QMessageBox::NoRole);
      msg.addButton("Cancel", QMessageBox::RejectRole);

      msg.exec();

      QString clicked = msg.clickedButton()->text();
      if(clicked == "Cancel")
        return false;
      else if(clicked == "Save Now")
      {
        m_klEditor->save();
        if(m_klEditor->hasUnsavedChanges())
          return false;
      }
    }

    m_uiController->setExec( execPath, exec );
  }
  catch ( FabricCore::Exception e )
  {
    printf( "Exception: %s\n", e.getDesc_cstr() );
    return false;
  }
  return true;  
}

void DFGWidget::editPropertiesForCurrentSelection()
{
  FabricUI::DFG::DFGController *controller = getUIController();
  if (controller)
  {
    std::vector<GraphView::Node *> nodes = getUIGraph()->selectedNodes();
    if (nodes.size() != 1)
    {
      if (nodes.size() == 0)  controller->log("cannot open node editor: no node selected.");
      else                    controller->log("cannot open node editor: more than one node selected.");
      return;
    }

    const char *nodeName = nodes[0]->name().c_str();
    FabricCore::DFGNodeType nodeType = controller->getExec().getNodeType( nodeName );
    if (   nodeType == FabricCore::DFGNodeType_Var
        || nodeType == FabricCore::DFGNodeType_Get
        || nodeType == FabricCore::DFGNodeType_Set)
    {
      controller->log("the node editor is not available for variable nodes.");
      return;
    }

    DFG::DFGNodePropertiesDialog dialog(NULL, controller, nodeName, getConfig());
    // [Julien] FE-5188, FE-5276, FE-5246
    if(dialog.exec())
    {
      controller->cmdSetNodeTitle       (nodeName, dialog.getTitle()  .toStdString().c_str());  // undoable.
      controller->setNodeToolTip        (nodeName, dialog.getToolTip().toStdString().c_str());  // not undoable.
      controller->setNodeDocUrl         (nodeName, dialog.getDocUrl() .toStdString().c_str());  // not undoable.

      controller->setNodeBackgroundColor(nodeName, dialog.getNodeColor());    // not undoable.  
      QColor headerColor;  
      if(dialog.getHeaderColor(headerColor)) controller->setNodeHeaderColor(nodeName, headerColor);  // not undoable.
      else controller->removeNodeHeaderColor(nodeName);                       // not undoable.
      controller->setNodeTextColor(nodeName, dialog.getTextColor());          // not undoable.
      // [Julien] FE-5246
      // Force update the header/nody node color
      onExecChanged();
    }
  }
}

QSettings * DFGWidget::getSettings()
{
  return g_settings;
}

void DFGWidget::setSettings(QSettings * settings)
{
  g_settings = settings;
}

void DFGWidget::refreshExtDeps( FTL::CStrRef extDeps )
{
  m_uiHeader->refreshExtDeps( extDeps );
}

void DFGWidget::populateMenuBar(QMenuBar * menuBar)
{
  QMenu *fileMenu = menuBar->addMenu(tr("&File"));
  QMenu *editMenu = menuBar->addMenu(tr("&Edit"));
  QMenu *viewMenu = menuBar->addMenu(tr("&View"));

  // emit the prefix menu entry requests
  emit additionalMenuActionsRequested("File", fileMenu, true);
  emit additionalMenuActionsRequested("Edit", editMenu, true);
  emit additionalMenuActionsRequested("View", viewMenu, true);

  // add separators if required
  if(fileMenu->actions().count() > 0)
    fileMenu->addSeparator();
  if(editMenu->actions().count() > 0)
    editMenu->addSeparator();
  if(viewMenu->actions().count() > 0)
    viewMenu->addSeparator();

  // edit menu
  QAction * cutAction = editMenu->addAction("Cut");
  QObject::connect(cutAction, SIGNAL(triggered()), this, SLOT(onCut()));
  QAction * copyAction = editMenu->addAction("Copy");
  QObject::connect(copyAction, SIGNAL(triggered()), this, SLOT(onCopy()));
  QAction * pasteAction = editMenu->addAction("Paste");
  QObject::connect(pasteAction, SIGNAL(triggered()), this, SLOT(onPaste()));

  // [Julien]  When using shortcut in Qt, set the flag WidgetWithChildrenShortcut
  // [Julien]  so the shortcut is specific to the widget
  // [Julien]  http://doc.qt.io/qt-4.8/qaction.html#shortcutContext-prop
  // [Julien]  http://doc.qt.io/qt-4.8/qt.html#ShortcutContext-enum
  cutAction->setShortcut( QKeySequence::Cut );
  cutAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  copyAction->setShortcut( QKeySequence::Copy );
  copyAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);
  pasteAction->setShortcut( QKeySequence::Paste );
  pasteAction->setShortcutContext(Qt::WidgetWithChildrenShortcut);

  // view menu
  QAction * dimLinesAction = viewMenu->addAction("Dim Connections");
  dimLinesAction->setCheckable(true);
  dimLinesAction->setChecked(m_uiGraph->config().dimConnectionLines);
  QObject::connect(dimLinesAction, SIGNAL(triggered()), this, SLOT(onToggleDimConnections()));
  QAction * portsCenteredAction = viewMenu->addAction("Side Ports Centered");
  portsCenteredAction->setCheckable(true);
  portsCenteredAction->setChecked(m_uiGraph->config().portsCentered);
  QObject::connect(portsCenteredAction, SIGNAL(triggered()), this, SLOT(onTogglePortsCentered()));

  // emit the suffix menu entry requests
  emit additionalMenuActionsRequested("File", fileMenu, false);
  emit additionalMenuActionsRequested("Edit", editMenu, false);
  emit additionalMenuActionsRequested("View", viewMenu, false);
}

void DFGWidget::onExecChanged()
{
  if ( m_router )
  {
    m_uiController->setRouter( 0 );
    delete m_router;
    m_router = 0;
  }

  FabricCore::DFGExec &exec = m_uiController->getExec();

  if ( exec.isValid() )
  {
    m_uiGraph = new GraphView::Graph( NULL, m_dfgConfig.graphConfig );
    m_uiGraph->setController(m_uiController.get());
    m_uiGraph->setEditable(m_isEditable);
    m_uiController->setGraph(m_uiGraph);

    if(m_isEditable)
    {
      QObject::connect(
        m_uiGraph, SIGNAL(hotkeyPressed(Qt::Key, Qt::KeyboardModifier, QString)), 
        this, SLOT(onHotkeyPressed(Qt::Key, Qt::KeyboardModifier, QString))
      );  
      QObject::connect(
        m_uiGraph, SIGNAL(hotkeyReleased(Qt::Key, Qt::KeyboardModifier, QString)), 
        this, SLOT(onHotkeyReleased(Qt::Key, Qt::KeyboardModifier, QString))
      );  
      QObject::connect(
        m_uiGraph, SIGNAL(bubbleEditRequested(FabricUI::GraphView::Node*)), 
        this, SLOT(onBubbleEditRequested(FabricUI::GraphView::Node*))
      );  
      m_uiGraph->defineHotkey(Qt::Key_Space, Qt::NoModifier, "PanGraph");
    }

    // [Julien] FE-5264
    // Before initializing the graph, sets the dimConnectionLines and portsCentered properties
    if(getSettings()) 
    {
      m_uiGraph->config().dimConnectionLines = getSettings()->value( "DFGWidget/dimConnectionLines").toBool();
      m_uiGraph->config().portsCentered = getSettings()->value( "DFGWidget/portsCentered").toBool();
    }
    m_uiGraph->initialize();

    m_router =
      static_cast<DFGNotificationRouter *>( m_uiController->createRouter() );
    m_uiController->setRouter(m_router);
  
    if(m_isEditable)
    {
      m_uiGraph->setGraphContextMenuCallback(&graphContextMenuCallback, this);
      m_uiGraph->setNodeContextMenuCallback(&nodeContextMenuCallback, this);
      m_uiGraph->setPortContextMenuCallback(&portContextMenuCallback, this);
      m_uiGraph->setSidePanelContextMenuCallback(&sidePanelContextMenuCallback, this);
    }

    if(exec.getType() == FabricCore::DFGExecType_Graph)
    {
      m_uiGraphViewWidget->show();
      m_uiGraphViewWidget->setFocus();
    }
    else if(exec.getType() == FabricCore::DFGExecType_Func)
    {
      m_uiGraphViewWidget->hide();
    }

    QString filePath = getenv("FABRIC_DIR");
    filePath += "/Resources/PoweredByFabric_black.png";
    m_uiGraph->setupBackgroundOverlay(QPointF(1, -70), filePath);

    // FE-4277
    emit onGraphSet(m_uiGraph);
  }
  else
  {
    m_uiGraph = NULL;
    m_uiController->setGraph(NULL);
  }

  m_uiGraphViewWidget->setGraph(m_uiGraph);

  if ( m_uiGraph )
  {
    try
    {
      m_router->onGraphSet();
    }
    catch(FabricCore::Exception e)
    {
      printf( "%s\n", e.getDesc_cstr() );
    }

    // FE-4277
    emit onGraphSet(m_uiGraph);
  }

  emit execChanged();
}
