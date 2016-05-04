#include <linux/acpi.h>
#include <acpi/button.h>

extern acpi_handle acpi_lid_handle;


extern acpi_status
AcpiGetDataFull(acpi_handle obj_handle, acpi_object_handler handler,
		void **data, void (*callback)(void *));

extern acpi_status
AcpiEvaluateObjectTyped(acpi_handle handle,
			acpi_string pathname,
			struct acpi_object_list *external_params,
			struct acpi_buffer *return_buffer,
			acpi_object_type return_type);

extern acpi_status
AcpiEvaluateObject(acpi_handle handle,
		   acpi_string pathname,
		   struct acpi_object_list *external_params,
		   struct acpi_buffer *return_buffer);

static void
acpi_util_eval_error(acpi_handle h, acpi_string p, acpi_status s)
{
#ifdef ACPI_DEBUG_OUTPUT
	char prefix[80] = {'\0'};
	struct acpi_buffer buffer = {sizeof(prefix), prefix};
	acpi_get_name(h, ACPI_FULL_PATHNAME, &buffer);
	ACPI_DEBUG_PRINT((ACPI_DB_INFO, "Evaluate [%s.%s]: %s\n",
		(char *) prefix, p, acpi_format_exception(s)));
#else
	return;
#endif
}

acpi_status
acpi_evaluate_integer(acpi_handle handle,
		      acpi_string pathname,
		      struct acpi_object_list *arguments, unsigned long long *data)
{
	acpi_status status = AE_OK;
	union acpi_object element;
	struct acpi_buffer buffer = { 0, NULL };

	if (!data)
		return AE_BAD_PARAMETER;

	buffer.length = sizeof(union acpi_object);
	buffer.pointer = &element;
	status = AcpiEvaluateObject(handle, pathname, arguments, &buffer);
	if (ACPI_FAILURE(status)) {
		acpi_util_eval_error(handle, pathname, status);
		return status;
	}

	if (element.type != ACPI_TYPE_INTEGER) {
		acpi_util_eval_error(handle, pathname, AE_BAD_DATA);
		return AE_BAD_DATA;
	}

	*data = element.integer.value;

	ACPI_DEBUG_PRINT((ACPI_DB_INFO, "Return value [%llu]\n", *data));

	return AE_OK;
}

acpi_status
acpi_evaluate_object_typed(acpi_handle handle,
			   acpi_string pathname,
			   struct acpi_object_list *external_params,
			   struct acpi_buffer *return_buffer,
			   acpi_object_type return_type)
{
	return (AcpiEvaluateObjectTyped(handle, pathname, external_params, return_buffer, return_type));
}

acpi_status
acpi_get_data_full(acpi_handle obj_handle, acpi_object_handler handler,
		   void **data, void (*callback)(void *))
{

	return (AcpiGetDataFull(obj_handle, handler, data, callback));
}

int
acpi_lid_open(void)
{
	acpi_status status;
	unsigned long long state;

	if (acpi_lid_handle == NULL)
		return (-ENODEV);

	status = acpi_evaluate_integer(acpi_lid_handle, "_LID", NULL,
				       &state);
	if (ACPI_FAILURE(status))
		return (-ENODEV);

	return (state != 0);
}
