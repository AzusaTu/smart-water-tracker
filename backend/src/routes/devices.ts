import { Router } from 'express';
import {
  bindDevice,
  listDevices,
  unbindDevice,
  getDeviceStatus,
} from '../controllers/deviceController';
import { authenticateUser } from '../middleware/auth';

const router = Router();

router.use(authenticateUser);

router.post('/', bindDevice);
router.get('/', listDevices);
router.delete('/:id', unbindDevice);
router.get('/:id/status', getDeviceStatus);

export default router;
