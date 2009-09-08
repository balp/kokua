/** 
 * @file llnavigationbar.cpp
 * @brief Navigation bar implementation
 *
 * $LicenseInfo:firstyear=2009&license=viewergpl$
 * 
 * Copyright (c) 2009, Linden Research, Inc.
 * 
 * Second Life Viewer Source Code
 * The source code in this file ("Source Code") is provided by Linden Lab
 * to you under the terms of the GNU General Public License, version 2.0
 * ("GPL"), unless you have obtained a separate licensing agreement
 * ("Other License"), formally executed by you and Linden Lab.  Terms of
 * the GPL can be found in doc/GPL-license.txt in this distribution, or
 * online at http://secondlifegrid.net/programs/open_source/licensing/gplv2
 * 
 * There are special exceptions to the terms and conditions of the GPL as
 * it is applied to this Source Code. View the full text of the exception
 * in the file doc/FLOSS-exception.txt in this software distribution, or
 * online at
 * http://secondlifegrid.net/programs/open_source/licensing/flossexception
 * 
 * By copying, modifying or distributing this software, you acknowledge
 * that you have read and understood your obligations described above,
 * and agree to abide by those obligations.
 * 
 * ALL LINDEN LAB SOURCE CODE IS PROVIDED "AS IS." LINDEN LAB MAKES NO
 * WARRANTIES, EXPRESS, IMPLIED OR OTHERWISE, REGARDING ITS ACCURACY,
 * COMPLETENESS OR PERFORMANCE.
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "llnavigationbar.h"

#include <llfloaterreg.h>
#include <llfocusmgr.h>
#include <lliconctrl.h>
#include <llmenugl.h>

#include "llagent.h"
#include "llviewerregion.h"
#include "lllandmarkactions.h"
#include "lllocationhistory.h"
#include "lllocationinputctrl.h"
#include "llteleporthistory.h"
#include "llsearchcombobox.h"
#include "llsidetray.h"
#include "llslurl.h"
#include "llurlsimstring.h"
#include "llviewerinventory.h"
#include "llviewerparcelmgr.h"
#include "llworldmap.h"
#include "llappviewer.h"
#include "llviewercontrol.h"
#include "llfloatermediabrowser.h"

#include "llinventorymodel.h"
#include "lllandmarkactions.h"

#include "llfavoritesbar.h"
#include "llagentui.h"

//-- LLTeleportHistoryMenuItem -----------------------------------------------

/**
 * Item look varies depending on the type (backward/current/forward). 
 */
class LLTeleportHistoryMenuItem : public LLMenuItemCallGL
{
public:
	typedef enum e_item_type
	{
		TYPE_BACKWARD,
		TYPE_CURRENT,
		TYPE_FORWARD,
	} EType;

	struct Params : public LLInitParam::Block<Params, LLMenuItemCallGL::Params>
	{
		Mandatory<EType> item_type;

		Params() {}
	};

	/*virtual*/ void	draw();
	/*virtual*/ void	onMouseEnter(S32 x, S32 y, MASK mask);
	/*virtual*/ void	onMouseLeave(S32 x, S32 y, MASK mask);

private:
	LLTeleportHistoryMenuItem(const Params&);
	friend class LLUICtrlFactory;

	static const S32			ICON_WIDTH			= 16;
	static const S32			ICON_HEIGHT			= 16;
	static const std::string	ICON_IMG_BACKWARD;
	static const std::string	ICON_IMG_FORWARD;

	LLIconCtrl*		mArrowIcon;
};

const std::string LLTeleportHistoryMenuItem::ICON_IMG_BACKWARD("teleport_history_backward.tga");
const std::string LLTeleportHistoryMenuItem::ICON_IMG_FORWARD("teleport_history_forward.tga");

LLTeleportHistoryMenuItem::LLTeleportHistoryMenuItem(const Params& p)
:	LLMenuItemCallGL(p),
	mArrowIcon(NULL)
{
	// Set appearance depending on the item type.
	if (p.item_type  == TYPE_CURRENT)
	{
		setFont(LLFontGL::getFontSansSerifBold());
	}
	else
	{
		setFont(LLFontGL::getFontSansSerif());
		setLabel(std::string("   ") + std::string(p.label));
	}

	LLIconCtrl::Params icon_params;
	icon_params.name("icon");
	icon_params.rect(LLRect(0, ICON_HEIGHT, ICON_WIDTH, 0));
	icon_params.mouse_opaque(false);
	icon_params.follows.flags(FOLLOWS_LEFT | FOLLOWS_TOP);
	icon_params.visible(false);

	mArrowIcon = LLUICtrlFactory::create<LLIconCtrl> (icon_params);

	// no image for the current item
	if (p.item_type == TYPE_BACKWARD)
		mArrowIcon->setValue(ICON_IMG_BACKWARD);
	else if (p.item_type == TYPE_FORWARD)
		mArrowIcon->setValue(ICON_IMG_FORWARD);

	addChild(mArrowIcon);
}

void LLTeleportHistoryMenuItem::draw()
{
	// Draw menu item itself.
	LLMenuItemCallGL::draw();

	// Draw children if any. *TODO: move this to LLMenuItemGL?
	LLUICtrl::draw();
}

void LLTeleportHistoryMenuItem::onMouseEnter(S32 x, S32 y, MASK mask)
{
	mArrowIcon->setVisible(TRUE);
}

void LLTeleportHistoryMenuItem::onMouseLeave(S32 x, S32 y, MASK mask)
{
	mArrowIcon->setVisible(FALSE);
}

//-- LNavigationBar ----------------------------------------------------------

/*
TODO:
- Load navbar height from saved settings (as it's done for status bar) or think of a better way.
*/

S32 NAVIGATION_BAR_HEIGHT = 60; // *HACK
LLNavigationBar* LLNavigationBar::sInstance = 0;

LLNavigationBar* LLNavigationBar::getInstance()
{
	if (!sInstance)
		sInstance = new LLNavigationBar();

	return sInstance;
}

LLNavigationBar::LLNavigationBar()
:	mTeleportHistoryMenu(NULL),
	mBtnBack(NULL),
	mBtnForward(NULL),
	mBtnHome(NULL),
	mCmbLocation(NULL),
	mSearchComboBox(NULL),
	mPurgeTPHistoryItems(false)
{
	setIsChrome(TRUE);
	
	LLUICtrlFactory::getInstance()->buildPanel(this, "panel_navigation_bar.xml");

	// set a listener function for LoginComplete event
	LLAppViewer::instance()->setOnLoginCompletedCallback(boost::bind(&LLNavigationBar::handleLoginComplete, this));

	// Necessary for focus movement among child controls
	setFocusRoot(TRUE);
}

LLNavigationBar::~LLNavigationBar()
{
	mTeleportFinishConnection.disconnect();
	sInstance = 0;

	LLSearchHistory::getInstance()->save();
}

BOOL LLNavigationBar::postBuild()
{
	mBtnBack	= getChild<LLButton>("back_btn");
	mBtnForward	= getChild<LLButton>("forward_btn");
	mBtnHome	= getChild<LLButton>("home_btn");
	
	mCmbLocation= getChild<LLLocationInputCtrl>("location_combo"); 
	mSearchComboBox	= getChild<LLSearchComboBox>("search_combo_box");

	fillSearchComboBox();

	if (!mBtnBack || !mBtnForward || !mBtnHome ||
		!mCmbLocation || !mSearchComboBox)
	{
		llwarns << "Malformed navigation bar" << llendl;
		return FALSE;
	}
	
	mBtnBack->setEnabled(FALSE);
	mBtnBack->setClickedCallback(boost::bind(&LLNavigationBar::onBackButtonClicked, this));
	mBtnBack->setHeldDownCallback(boost::bind(&LLNavigationBar::onBackOrForwardButtonHeldDown, this, _2));

	mBtnForward->setEnabled(FALSE);
	mBtnForward->setClickedCallback(boost::bind(&LLNavigationBar::onForwardButtonClicked, this));
	mBtnForward->setHeldDownCallback(boost::bind(&LLNavigationBar::onBackOrForwardButtonHeldDown, this, _2));

	mBtnHome->setClickedCallback(boost::bind(&LLNavigationBar::onHomeButtonClicked, this));

	mCmbLocation->setSelectionCallback(boost::bind(&LLNavigationBar::onLocationSelection, this));
	
	mSearchComboBox->setCommitCallback(boost::bind(&LLNavigationBar::onSearchCommit, this));

	mDefaultNbRect = getRect();
	mDefaultFpRect = getChild<LLFavoritesBarCtrl>("favorite")->getRect();

	// we'll be notified on teleport history changes
	LLTeleportHistory::getInstance()->setHistoryChangedCallback(
			boost::bind(&LLNavigationBar::onTeleportHistoryChanged, this));

	return TRUE;
}

void LLNavigationBar::fillSearchComboBox()
{
	if(!mSearchComboBox)
	{
		return;
	}

	LLSearchHistory::getInstance()->load();

	LLSearchHistory::search_history_list_t search_list = 
		LLSearchHistory::getInstance()->getSearchHistoryList();
	LLSearchHistory::search_history_list_t::const_iterator it = search_list.begin();
	for( ; search_list.end() != it; ++it)
	{
		LLSearchHistory::LLSearchHistoryItem item = *it;
		mSearchComboBox->add(item.search_query);
	}
}

void LLNavigationBar::draw()
{
	if(mPurgeTPHistoryItems)
	{
		LLTeleportHistory::getInstance()->purgeItems();
		onTeleportHistoryChanged();
		mPurgeTPHistoryItems = false;
	}
	LLPanel::draw();
}

void LLNavigationBar::onBackButtonClicked()
{
	LLTeleportHistory::getInstance()->goBack();
}

void LLNavigationBar::onBackOrForwardButtonHeldDown(const LLSD& param)
{
	if (param["count"].asInteger() == 0)
		showTeleportHistoryMenu();
}

void LLNavigationBar::onForwardButtonClicked()
{
	LLTeleportHistory::getInstance()->goForward();
}

void LLNavigationBar::onHomeButtonClicked()
{
	gAgent.teleportHome();
}

void LLNavigationBar::onSearchCommit()
{
	std::string search_query = mSearchComboBox->getValue().asString();
	if(!search_query.empty())
	{
		LLSearchHistory::getInstance()->addEntry(search_query);
		invokeSearch(mSearchComboBox->getValue().asString());	
	}
}

void LLNavigationBar::onTeleportHistoryMenuItemClicked(const LLSD& userdata)
{
	int idx = userdata.asInteger();
	LLTeleportHistory::getInstance()->goToItem(idx);
}

// This is called when user presses enter in the location input
// or selects a location from the typed locations dropdown.
void LLNavigationBar::onLocationSelection()
{
	std::string typed_location = mCmbLocation->getSimple();

	// Will not teleport to empty location.
	if (typed_location.empty())
		return;

	LLSD value = mCmbLocation->getSelectedValue();
	
	if(value.has("item_type"))
	{

		switch(value["item_type"].asInteger())
		{
		case LANDMARK:
			
			if(value.has("AssetUUID"))
			{
				
				gAgent.teleportViaLandmark( LLUUID(value["AssetUUID"].asString()));
				return;
			}
			else
			{
				LLInventoryModel::item_array_t landmark_items =
						LLLandmarkActions::fetchLandmarksByName(typed_location,
								FALSE);
				if (!landmark_items.empty())
				{
					gAgent.teleportViaLandmark( landmark_items[0]->getAssetUUID());
					return; 
				}
			}
			break;
			
		case TELEPORT_HISTORY:
			//in case of teleport item was selected, teleport by position too.
		case TYPED_REGION_SURL:
			if(value.has("global_pos"))
			{
				gAgent.teleportViaLocation(LLVector3d(value["global_pos"]));
				return;
			}
			break;
			
		default:
			break;		
		}
	}
	//Let's parse surl or region name
	
	std::string region_name;
	LLVector3 local_coords(128, 128, 0);
	S32 x = 0, y = 0, z = 0;
	// Is the typed location a SLURL?
	if (LLSLURL::isSLURL(typed_location))
	{
		// Yes. Extract region name and local coordinates from it.
		if (LLURLSimString::parse(LLSLURL::stripProtocol(typed_location), &region_name, &x, &y, &z))
				local_coords.set(x, y, z);
		else
			return;
	}else
	{
		// assume that an user has typed the {region name} or possible {region_name, parcel}
		region_name  = typed_location.substr(0,typed_location.find(','));
	}
	
	// Resolve the region name to its global coordinates.
	// If resolution succeeds we'll teleport.
	LLWorldMap::url_callback_t cb = boost::bind(
			&LLNavigationBar::onRegionNameResponse, this,
			typed_location, region_name, local_coords, _1, _2, _3, _4);
	// connect the callback each time, when user enter new location to get real location of agent after teleport
	mTeleportFinishConnection = LLViewerParcelMgr::getInstance()->
			setTeleportFinishedCallback(boost::bind(&LLNavigationBar::onTeleportFinished, this, _1,typed_location));
	
	LLWorldMap::getInstance()->sendNamedRegionRequest(region_name, cb, std::string("unused"), false);
}

void LLNavigationBar::onTeleportFinished(const LLVector3d& global_agent_pos, const std::string& typed_location)
{
	// Location is valid. Add it to the typed locations history.
	LLLocationHistory* lh = LLLocationHistory::getInstance();

	//TODO*: do we need convert surl into readable format?
	std::string location;
	/*NOTE:
	 * We can't use gAgent.getPositionAgent() in case of local teleport to build location.
	 * At this moment gAgent.getPositionAgent() contains previous coordinates.
	 * according to EXT-65 agent position is being reseted on each frame.  
	 */
		LLAgentUI::buildLocationString(location, LLAgentUI::LOCATION_FORMAT_WITHOUT_SIM,
					gAgent.getPosAgentFromGlobal(global_agent_pos));
	std::string tooltip (LLSLURL::buildSLURLfromPosGlobal(gAgent.getRegion()->getName(), global_agent_pos, false));
	
	LLLocationHistoryItem item (location,
			global_agent_pos, tooltip,TYPED_REGION_SURL);// we can add into history only TYPED location
	//Touch it, if it is at list already, add new location otherwise
	if ( !lh->touchItem(item) ) {
		lh->addItem(item);
	}

	lh->save();
	
	if(mTeleportFinishConnection.connected())
		mTeleportFinishConnection.disconnect();
	
}

void LLNavigationBar::onTeleportHistoryChanged()
{
	// Update navigation controls.
	LLTeleportHistory* h = LLTeleportHistory::getInstance();
	int cur_item = h->getCurrentItemIndex();
	mBtnBack->setEnabled(cur_item > 0);
	mBtnForward->setEnabled(cur_item < ((int)h->getItems().size() - 1));
}

void LLNavigationBar::rebuildTeleportHistoryMenu()
{
	// Has the pop-up menu been built?
	if (mTeleportHistoryMenu)
	{
		// Clear it.
		mTeleportHistoryMenu->empty();
	}
	else
	{
		// Create it.
		LLMenuGL::Params menu_p;
		menu_p.name("popup");
		menu_p.can_tear_off(false);
		menu_p.visible(false);
		menu_p.bg_visible(true);
		menu_p.scrollable(true);
		mTeleportHistoryMenu = LLUICtrlFactory::create<LLMenuGL>(menu_p);
		
		addChild(mTeleportHistoryMenu);
	}
	
	// Populate the menu with teleport history items.
	LLTeleportHistory* hist = LLTeleportHistory::getInstance();
	const LLTeleportHistory::slurl_list_t& hist_items = hist->getItems();
	int cur_item = hist->getCurrentItemIndex();
	
	// Items will be shown in the reverse order, just like in Firefox.
	for (int i = (int)hist_items.size()-1; i >= 0; i--)
	{
		LLTeleportHistoryMenuItem::EType type;
		if (i < cur_item)
			type = LLTeleportHistoryMenuItem::TYPE_BACKWARD;
		else if (i > cur_item)
			type = LLTeleportHistoryMenuItem::TYPE_FORWARD;
		else
			type = LLTeleportHistoryMenuItem::TYPE_CURRENT;

		LLTeleportHistoryMenuItem::Params item_params;
		item_params.label = item_params.name = hist_items[i].getTitle();
		item_params.item_type = type;
		item_params.on_click.function(boost::bind(&LLNavigationBar::onTeleportHistoryMenuItemClicked, this, i));
		LLTeleportHistoryMenuItem* new_itemp = LLUICtrlFactory::create<LLTeleportHistoryMenuItem>(item_params);
		//new_itemp->setFont()
		mTeleportHistoryMenu->addChild(new_itemp);
	}
}

void LLNavigationBar::onRegionNameResponse(
		std::string typed_location,
		std::string region_name,
		LLVector3 local_coords,
		U64 region_handle, const std::string& url, const LLUUID& snapshot_id, bool teleport)
{
	// Invalid location?
	if (!region_handle)
	{
		invokeSearch(typed_location);
		return;
	}

	// Teleport to the location.
	LLVector3d region_pos = from_region_handle(region_handle);
	LLVector3d global_pos = region_pos + (LLVector3d) local_coords;

	llinfos << "Teleporting to: " << LLSLURL::buildSLURLfromPosGlobal(region_name,	global_pos, false)  << llendl;
	gAgent.teleportViaLocation(global_pos);
}

void	LLNavigationBar::showTeleportHistoryMenu()
{
	// Don't show the popup if teleport history is empty.
	if (LLTeleportHistory::getInstance()->isEmpty())
	{
		lldebugs << "Teleport history is empty, will not show the menu." << llendl;
		return;
	}
	
	rebuildTeleportHistoryMenu();

	if (mTeleportHistoryMenu == NULL)
		return;
	
	// *TODO: why to draw/update anything before showing the menu?
	mTeleportHistoryMenu->buildDrawLabels();
	mTeleportHistoryMenu->updateParent(LLMenuGL::sMenuContainer);
	LLRect btnBackRect = mBtnBack->getRect();
	LLMenuGL::showPopup(this, mTeleportHistoryMenu, btnBackRect.mLeft, btnBackRect.mBottom);

	// *HACK pass the mouse capturing to the drop-down menu
	gFocusMgr.setMouseCapture( NULL );
}

void LLNavigationBar::handleLoginComplete()
{
	mCmbLocation->handleLoginComplete();
}

void LLNavigationBar::invokeSearch(std::string search_text)
{
	LLFloaterReg::showInstance("search", LLSD().insert("panel", "all").insert("id", LLSD(search_text)));
}

void LLNavigationBar::clearHistoryCache()
{
	mCmbLocation->removeall();
	LLLocationHistory* lh = LLLocationHistory::getInstance();
	lh->removeItems();
	lh->save();	
	mPurgeTPHistoryItems= true;
}

void LLNavigationBar::showNavigationPanel(BOOL visible)
{
	bool fpVisible = gSavedSettings.getBOOL("ShowNavbarFavoritesPanel");

	LLFavoritesBarCtrl* fb = getChild<LLFavoritesBarCtrl>("favorite");
	LLPanel* navPanel = getChild<LLPanel>("navigation_panel");

	LLRect nbRect(getRect());
	LLRect fbRect(fb->getRect());

	navPanel->setVisible(visible);

	if (visible)
	{
		if (fpVisible)
		{
			// Navigation Panel must be shown. Favorites Panel is visible.

			nbRect.setLeftTopAndSize(nbRect.mLeft, nbRect.mTop, nbRect.getWidth(), mDefaultNbRect.getHeight());
			fbRect.setLeftTopAndSize(fbRect.mLeft, mDefaultFpRect.mTop, fbRect.getWidth(), fbRect.getHeight());

			// this is duplicated in 'else' section because it should be called BEFORE fb->reshape
			reshape(nbRect.getWidth(), nbRect.getHeight());
			setRect(nbRect);

			fb->reshape(fbRect.getWidth(), fbRect.getHeight());
			fb->setRect(fbRect);
		}
		else
		{
			// Navigation Panel must be shown. Favorites Panel is hidden.

			S32 height = mDefaultNbRect.getHeight() - mDefaultFpRect.getHeight();
			nbRect.setLeftTopAndSize(nbRect.mLeft, nbRect.mTop, nbRect.getWidth(), height);

			reshape(nbRect.getWidth(), nbRect.getHeight());
			setRect(nbRect);
		}
	}
	else
	{
		if (fpVisible)
		{
			// Navigation Panel must be hidden. Favorites Panel is visible.

			nbRect.setLeftTopAndSize(nbRect.mLeft, nbRect.mTop, nbRect.getWidth(), fbRect.getHeight());
			fbRect.setLeftTopAndSize(fbRect.mLeft, fbRect.getHeight(), fbRect.getWidth(), fbRect.getHeight());

			// this is duplicated in 'else' section because it should be called BEFORE fb->reshape
			reshape(nbRect.getWidth(), nbRect.getHeight());
			setRect(nbRect);

			fb->reshape(fbRect.getWidth(), fbRect.getHeight());
			fb->setRect(fbRect);
		}
		else
		{
			// Navigation Panel must be hidden. Favorites Panel is hidden.

			nbRect.setLeftTopAndSize(nbRect.mLeft, nbRect.mTop, nbRect.getWidth(), 0);

			reshape(nbRect.getWidth(), nbRect.getHeight());
			setRect(nbRect);
		}
	}

	if(LLSideTray::instanceCreated())
	{
		LLSideTray::getInstance()->resetPanelRect();
	}
}

void LLNavigationBar::showFavoritesPanel(BOOL visible)
{
	bool npVisible = gSavedSettings.getBOOL("ShowNavbarNavigationPanel");

	LLFavoritesBarCtrl* fb = getChild<LLFavoritesBarCtrl>("favorite");

	LLRect nbRect(getRect());
	LLRect fbRect(fb->getRect());

	if (visible)
	{
		if (npVisible)
		{
			// Favorites Panel must be shown. Navigation Panel is visible.

			S32 fbHeight = fbRect.getHeight();
			S32 newHeight = nbRect.getHeight() + fbHeight;

			nbRect.setLeftTopAndSize(nbRect.mLeft, nbRect.mTop, nbRect.getWidth(), newHeight);
			fbRect.setLeftTopAndSize(mDefaultFpRect.mLeft, mDefaultFpRect.mTop, fbRect.getWidth(), fbRect.getHeight());
		}
		else
		{
			// Favorites Panel must be shown. Navigation Panel is hidden.

			S32 fpHeight = mDefaultFpRect.getHeight();
			nbRect.setLeftTopAndSize(nbRect.mLeft, nbRect.mTop, nbRect.getWidth(), fpHeight);
			fbRect.setLeftTopAndSize(fbRect.mLeft, fpHeight, fbRect.getWidth(), fpHeight);
		}

		reshape(nbRect.getWidth(), nbRect.getHeight());
		setRect(nbRect);

		fb->reshape(fbRect.getWidth(), fbRect.getHeight());
		fb->setRect(fbRect);
	}
	else
	{
		if (npVisible)
		{
			// Favorites Panel must be hidden. Navigation Panel is visible.

			S32 fbHeight = fbRect.getHeight();
			S32 newHeight = nbRect.getHeight() - fbHeight;

			nbRect.setLeftTopAndSize(nbRect.mLeft, nbRect.mTop, nbRect.getWidth(), newHeight);
		}
		else
		{
			// Favorites Panel must be hidden. Navigation Panel is hidden.

			nbRect.setLeftTopAndSize(nbRect.mLeft, nbRect.mTop, nbRect.getWidth(), 0);
		}

		reshape(nbRect.getWidth(), nbRect.getHeight());
		setRect(nbRect);
	}

	fb->setVisible(visible);
	if(LLSideTray::instanceCreated())
	{
		LLSideTray::getInstance()->resetPanelRect();
	}
}
