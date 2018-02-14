/* --------------------------------------------------------------------------------
 #
 #	4DPlugin.cpp
 #	source generated by 4D Plugin Wizard
 #	Project : dmp
 #	author : miyako
 #	2018/02/14
 #
 # --------------------------------------------------------------------------------*/


#include "4DPluginAPI.h"
#include "4DPlugin.h"

void PluginMain(PA_long32 selector, PA_PluginParameters params)
{
	try
	{
		PA_long32 pProcNum = selector;
		sLONG_PTR *pResult = (sLONG_PTR *)params->fResult;
		PackagePtr pParams = (PackagePtr)params->fParameters;

		CommandDispatcher(pProcNum, pResult, pParams); 
	}
	catch(...)
	{

	}
}

void CommandDispatcher (PA_long32 pProcNum, sLONG_PTR *pResult, PackagePtr pParams)
{
	switch(pProcNum)
	{
// --- new theme1

		case 1 :
			diff_match_patch(pResult, pParams);
			break;

	}
}

// ---------------------------------- new theme1 ----------------------------------

void json_wconv(const char *value, std::wstring &u32)
{
	if((value) && strlen(value))
	{
		C_TEXT t;
		CUTF8String u8 = CUTF8String((const uint8_t *)value);
		
		t.setUTF8String(&u8);
		
#if VERSIONWIN
		u32 = std::wstring((wchar_t *)t.getUTF16StringPtr());
#else
		uint32_t dataSize = (t.getUTF16Length() * sizeof(wchar_t))+ sizeof(wchar_t);
		std::vector<char> buf(dataSize);
		
		PA_ConvertCharsetToCharset((char *)t.getUTF16StringPtr(),
															 t.getUTF16Length() * sizeof(PA_Unichar),
															 eVTC_UTF_16,
															 (char *)&buf[0],
															 dataSize,
															 eVTC_UTF_32);
		
		u32 = std::wstring((wchar_t *)&buf[0]);
#endif
	}else
	{
		u32 = L"";
	}
	
}

void diff_match_patch(sLONG_PTR *pResult, PackagePtr pParams)
{
	C_TEXT Param1_text_1;
	C_TEXT Param2_text_2;
	C_TEXT Param3_option;
	C_TEXT returnValue;

	Param1_text_1.fromParamAtIndex(pParams, 1);
	Param2_text_2.fromParamAtIndex(pParams, 2);
	Param3_option.fromParamAtIndex(pParams, 3);

	/* because we used the same name for function */
	class diff_match_patch dmp;
	
	CUTF8String option_u8;
	Param3_option.copyUTF8String(&option_u8);
	
	std::wstring option_w;
	json_wconv((const char *)option_u8.c_str(), option_w);
	
	JSONNODE *option = json_parse(option_w.c_str());
	if(option)
	{
		/* diff options */
		JSONNODE *diffTimeout = json_get(option, L"diffTimeout");
		if(diffTimeout)
		{
			dmp.Diff_Timeout = json_as_float(diffTimeout);
		}
		JSONNODE *diffEditCost = json_get(option, L"diffEditCost");
		if(diffEditCost)
		{
			dmp.Diff_EditCost = json_as_int(diffEditCost);
		}
		
		/* patch options */
		JSONNODE *patchMargin = json_get(option, L"patchMargin");
		if(patchMargin)
		{
			dmp.Patch_Margin = json_as_int(patchMargin);
		}
		JSONNODE *patchDeleteThreshold = json_get(option, L"patchDeleteThreshold");
		if(patchDeleteThreshold)
		{
			dmp.Patch_DeleteThreshold = json_as_float(patchDeleteThreshold);
		}
	
		/* match options */
		JSONNODE *matchThreshold = json_get(option, L"matchThreshold");
		if(matchThreshold)
		{
			dmp.Match_Threshold = json_as_float(matchThreshold);

		}
		JSONNODE *matchDistance = json_get(option, L"matchDistance");
		if(matchDistance)
		{
			dmp.Match_Distance = json_as_int(matchDistance);
		}
		JSONNODE *matchMaxBits = json_get(option, L"matchMaxBits");
		if(matchMaxBits)
		{
			dmp.Match_MaxBits = json_as_int(matchMaxBits);

		}
		json_delete(option);
	}/* json_parse */
	
	/*
	NSLog(@"Diff_Timeout:%f", dmp.Diff_Timeout);
	NSLog(@"Diff_EditCost:%i", dmp.Diff_EditCost);
	NSLog(@"Patch_Margin:%i", dmp.Patch_Margin);
	NSLog(@"Patch_DeleteThreshold:%f", dmp.Patch_DeleteThreshold);
	NSLog(@"Match_Threshold:%f", dmp.Match_Threshold);
	NSLog(@"Match_Distance:%i", dmp.Match_Distance);
	NSLog(@"Match_MaxBits:%i", dmp.Match_MaxBits);
	 */
	
	QString t1((const QChar *)Param1_text_1.getUTF16StringPtr(), Param1_text_1.getUTF16Length());
	QString t2((const QChar *)Param2_text_2.getUTF16StringPtr(), Param2_text_2.getUTF16Length());
	
	QList<Diff>diffs = dmp.diff_main(t1, t2);

	QString html = dmp.diff_prettyHtml(diffs);
	
	returnValue.setUTF16String((const PA_Unichar *)html.data(), html.length());
	returnValue.setReturn(pResult);
}

