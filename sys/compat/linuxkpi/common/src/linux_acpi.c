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
AcpiEvaluateInteger(acpi_handle handle,
		   acpi_string pathname,
		    struct acpi_object_list *arguments, unsigned long long *data);


acpi_status
acpi_evaluate_integer(acpi_handle handle,
		      acpi_string pathname,
		      struct acpi_object_list *arguments, unsigned long long *data)
{
	return (AcpiEvaluateInteger(handle, pathname, arguments, data));

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
