#include "stdafx.h"
#include "xTerrainPane.h"
#include "xApp.h"
#include "xAfxResourceSetup.h"
#include "resource.h"


#define xHeightPage 0
#define xLayerPage 1
#define xVegPage 2

//////////////////////////////////////////////////////////////////////////
//
// Terrain Pane
//
xTerrainPane gTerrainPane;

BEGIN_MESSAGE_MAP(xTerrainPane, CDockablePane)
	ON_WM_CREATE()
	ON_WM_SIZE()
	ON_WM_CONTEXTMENU()

END_MESSAGE_MAP()

IMP_SLN(xTerrainPane);

xTerrainPane::xTerrainPane()
	: IDockPane("Terrain") 
	, OnCreatePane(xEvent::OnCreatePane, this, &xTerrainPane::_Create)
	, OnInit(xEvent::OnInitUI, this, &xTerrainPane::_Init)
	, OnShutdown(xEvent::OnShutdown, this, &xTerrainPane::_Shutdown)
	, OnUpdate(xEvent::OnUpdate, this, &xTerrainPane::_Update)
	, OnRender(RenderEvent::OnAfterDefferedShading, this, &xTerrainPane::_Render)
	, OnRenderUI(RenderEvent::OnAfterRender, this, &xTerrainPane::_RenderUI)
	, OnUnLoadScene(xEvent::OnUnloadScene, this, &xTerrainPane::_UnloadScene)
	, OnAfterLoadScene(xEvent::OnAfterLoadScene, this, &xTerrainPane::_AfterloadScene)
{
	INIT_SLN;
}

xTerrainPane::~xTerrainPane()
{
	SHUT_SLN;
}

int xTerrainPane::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CDockablePane::OnCreate(lpCreateStruct) == -1)
		return -1;

	afx_resource_setup();

	// create tab control
	CRect rectDummy;
	rectDummy.SetRectEmpty();

	if (!mTab.Create(CMFCTabCtrl::STYLE_3D_VS2005, rectDummy, this, IDC_Terrain_Tab))
	{
		TRACE0("Create \"Terrain Tab\" failed!\n");
		return -1;
	}

	mTab.SetLocation(CMFCBaseTabCtrl::LOCATION_TOP);

	// create height dialog
	if (!mHeightDlg.Create(IDD_Terrain_Height, &mTab))
	{
		TRACE0("Create \"Terrain Height Dialog\" failed!\n");
		return -1;
	}

	// create layer dialog
	if (!mLayerDlg.Create(IDD_Terrain_Layer, &mTab))
	{
		TRACE0("Create \"Terrain Layer Dialog\" failed!\n");
		return -1;
	}

	// create vegetation dialog
	if (!mVegDlg.Create(IDD_Terrain_Veg, &mTab))
	{
		TRACE0("Create \"Terrain Vegetation Dialog\" failed!\n");
		return -1;
	}

	mTab.AddTab(&mHeightDlg, "Height");
	mTab.AddTab(&mLayerDlg, "Layer");
	mTab.AddTab(&mVegDlg, "Vegetation");

	AdjustLayout();

	return 0;
}

void xTerrainPane::OnSize(UINT nType, int cx, int cy)
{
	CDockablePane::OnSize(nType, cx, cy);
	AdjustLayout();
}

void xTerrainPane::AdjustLayout()
{
	if (GetSafeHwnd() == NULL)
		return;

	CRect rectClient,rectCombo;
	GetClientRect(rectClient);

	int cyCmb = rectCombo.Size().cy;
	int cyTlb = 0;

	mTab.SetWindowPos(NULL, rectClient.left, rectClient.top + cyCmb + cyTlb, rectClient.Width(), rectClient.Height() -(cyCmb+cyTlb), SWP_NOACTIVATE | SWP_NOZORDER);

	CRect rc;   
	mTab.GetClientRect(rc);   
	rc.top += 20;   
	rc.bottom -= 5;   
	rc.left += 5;   
	rc.right -= 5;

	mHeightDlg.MoveWindow(&rc);
	mLayerDlg.MoveWindow(&rc);
}

void xTerrainPane::_Create(Event * sender)
{
	CFrameWndEx * frame = (CFrameWndEx *)sender->GetParam(0);

	if (!Create("Terrain", frame, CRect(0, 0, 200, 200), TRUE, IDD_Terrain, 
		WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | CBRS_RIGHT | CBRS_FLOAT_MULTI))
	{
		TRACE0("can't create \"Terrain pane\"\n");
		return ;
	}

	EnableDocking(CBRS_ALIGN_ANY);
	frame->DockPane(this);

	ShowPane(FALSE, FALSE, TRUE);
}

void xTerrainPane::_Init(Event * sender)
{
	mEditHeight._Init(NULL);
	mEditLayer._Init(NULL);
	mEditVeg._Init(NULL);

	mHeightDlg._Init(NULL);
	mLayerDlg._Init(NULL);
	mVegDlg._Init(NULL);
}

void xTerrainPane::_Shutdown(Event * sender)
{
	mEditHeight._Shutdown(NULL);
	mEditLayer._Shutdown(NULL);
	mEditVeg._Shutdown(NULL);
}

void xTerrainPane::_Update(Event * sender)
{
	int op = xApp::Instance()->GetOperator();

	if (op != xTerrainOp::eOp_Terrain)
		return ;

	int layerId = mLayerDlg.GetCurLayer();
	mEditLayer.SetLayer(layerId);

	int isel = mTab.GetActiveTab();

	if (isel == xHeightPage)
	{
		mEditHeight._Update(NULL);
	}
	else if (isel == xLayerPage)
	{
		mEditLayer._Update(NULL);
	}
	else if (isel == xVegPage)
	{
		mEditVeg._Update(NULL);
	}
}

void xTerrainPane::_Render(Event * sender)
{
	int op = xApp::Instance()->GetOperator();

	if (op != xTerrainOp::eOp_Terrain)
		return ;

	int isel = mTab.GetActiveTab();

	if (isel == xHeightPage)
	{
		mEditHeight._Render(NULL);
	}
	else if (isel == xLayerPage)
	{
		mEditLayer._Render(NULL);
	}
	else if (isel == xVegPage)
	{
		mEditVeg._Render(NULL);
	}
}

void xTerrainPane::_RenderUI(Event * sender)
{
	int op = xApp::Instance()->GetOperator();

	if (op != xTerrainOp::eOp_Terrain)
		return ;

	int isel = mTab.GetActiveTab();

	if (isel == xHeightPage)
	{
	}
	else if (isel == xLayerPage)
	{
		mEditLayer._RenderSectionLayer();
	}
}

void xTerrainPane::_UnloadScene(Event * sender)
{
	mEditLayer.SetLayer(-1);
	mLayerDlg._UnloadScene(NULL);
	mVegDlg._UnloadScene(NULL);
}

void xTerrainPane::_AfterloadScene(Event * sender)
{
	mEditLayer.SetLayer(-1);
	mLayerDlg._AfterLoadScene(NULL);
	mVegDlg._AfterLoadScene(NULL);
}