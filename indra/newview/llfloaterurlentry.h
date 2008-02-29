/**
 * @file llfloaternamedesc.h
 * @brief LLFloaterNameDesc class definition
 *
 * $LicenseInfo:firstyear=2007&license=viewergpl$
 *
 * Copyright (c) 2007-2008, Linden Research, Inc.
 *
 * Second Life Viewer Source Code
 * The source code in this file ("Source Code") is provided by Linden Lab
 * to you under the terms of the GNU General Public License, version 2.0
 * ("GPL"), unless you have obtained a separate licensing agreement
 * ("Other License"), formally executed by you and Linden Lab.  Terms of
 * the GPL can be found in doc/GPL-license.txt in this distribution, or
 * online at http://secondlife.com/developers/opensource/gplv2
 *
 * There are special exceptions to the terms and conditions of the GPL as
 * it is applied to this Source Code. View the full text of the exception
 * in the file doc/FLOSS-exception.txt in this software distribution, or
 * online at http://secondlife.com/developers/opensource/flossexception
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

#ifndef LL_LLFLOATERURLENTRY_H
#define LL_LLFLOATERURLENTRY_H

#include "llfloater.h"
#include "llpanellandmedia.h"

class LLLineEditor;
class LLComboBox;

class LLFloaterURLEntry : public LLFloater
{
public:
	// Can only be shown by LLPanelLandMedia, and pushes data back into
	// that panel via the handle.
	static LLHandle<LLFloater> show(LLHandle<LLPanel> panel_land_media_handle);

	void updateFromLandMediaPanel();

	void headerFetchComplete(U32 status, const std::string& mime_type);

	bool addURLToCombobox(const std::string& media_url);

private:
	LLFloaterURLEntry(LLHandle<LLPanel> parent);
	/*virtual*/ ~LLFloaterURLEntry();
	void buildURLHistory();

private:
	LLComboBox*		mMediaURLEdit;
	LLHandle<LLPanel> mPanelLandMediaHandle;

	static void		onBtnOK(void*);
	static void		onBtnCancel(void*);
	static void		onBtnClear(void*);
	static void		callback_clear_url_list(S32 option, void* userdata);
};

#endif  // LL_LLFLOATERURLENTRY_H