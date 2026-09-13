// The stock CS_DIALOG blocks pan/drawing, but in 1.8.0.13 it does not block
// wheel zoom, selection or tool actions. Suppress those only for this owned form.
// Регистрация нужна и для modded класса: stock Map*.conf создают этот module.
[BaseContainerProps()]
modded class SCR_MapCursorModule
{
	protected AICF_SupplyMapUI m_AICFSupplyDialog;

	void AICF_SetSupplyDialog(AICF_SupplyMapUI dialog)
	{
		m_AICFSupplyDialog = dialog;
		HandleDialog(true);
	}

	void AICF_ClearSupplyDialog(AICF_SupplyMapUI dialog)
	{
		if (m_AICFSupplyDialog != dialog)
			return;
		m_AICFSupplyDialog = null;
		HandleDialog(false);
	}

	protected bool AICF_HasSupplyDialog()
	{
		return m_AICFSupplyDialog && m_AICFSupplyDialog.IsInputCaptured();
	}

	override protected void OnInputZoomIn(float value, EActionTrigger reason)
	{
		if (!AICF_HasSupplyDialog())
			super.OnInputZoomIn(value, reason);
	}

	override protected void OnInputZoomOut(float value, EActionTrigger reason)
	{
		if (!AICF_HasSupplyDialog())
			super.OnInputZoomOut(value, reason);
	}

	override protected void OnInputZoomWheelUp(float value, EActionTrigger reason)
	{
		if (!AICF_HasSupplyDialog())
			super.OnInputZoomWheelUp(value, reason);
	}

	override protected void OnInputZoomWheelDown(float value, EActionTrigger reason)
	{
		if (!AICF_HasSupplyDialog())
			super.OnInputZoomWheelDown(value, reason);
	}

	override protected void OnInputModifClick(float value, EActionTrigger reason)
	{
		if (!AICF_HasSupplyDialog())
			super.OnInputModifClick(value, reason);
	}

	override protected void HandleSelect()
	{
		if (!AICF_HasSupplyDialog())
			super.HandleSelect();
	}

	override protected void HandleMultiSelect(bool activate)
	{
		if (!AICF_HasSupplyDialog())
			super.HandleMultiSelect(activate);
	}

	override protected void HandleDrag(bool startDrag)
	{
		if (!AICF_HasSupplyDialog())
			super.HandleDrag(startDrag);
	}

	override protected void HandleRotateTool(bool startRotate)
	{
		if (!AICF_HasSupplyDialog())
			super.HandleRotateTool(startRotate);
	}

	override bool HandleContextualMenu(bool doClose = false)
	{
		if (AICF_HasSupplyDialog() && !doClose)
			return false;
		return super.HandleContextualMenu(doClose);
	}
}
