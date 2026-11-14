from osgeo import gdal
d = gdal.Open('verification/wcs/ahn_small.tif')
print('size', d.RasterXSize, d.RasterYSize, 'bands', d.RasterCount)
print('geotransform', d.GetGeoTransform())
p = d.GetProjectionRef() or ''
print('proj tail', repr(p[-70:]))
b = d.GetRasterBand(1)
print('dtype', gdal.GetDataTypeName(b.DataType))
print('min,max,mean,std', [round(v, 3) for v in b.ComputeStatistics(False)])
